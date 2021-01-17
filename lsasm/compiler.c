#include "compiler.h"

#include "buffer.h"
#include <internal/types.h>
#include <internal/datau.h>
#include <internal/lb.h>
#include <stdio.h>

typedef struct line_s line_t;
struct line_s
{
	char *line;
	line_t *next;
	int linenum;
};

static compile_error_t *compile_file(const char *file, const char *outputFile, compile_error_t *back);
static compile_error_t *compile_data(const char *data, size_t datalen, buffer_t *out, const char *srcFile, compile_error_t *back);
static line_t *format_document(const char *data, size_t datalen);
static void free_formatted(line_t *first);
static byte_t get_command_byte(const char *string);
static char **tokenize_string(const char *string, size_t *tokenCount);
static void free_tokenized_data(char **data, size_t tokenCount);
static size_t evaluate_constant(const char *string, data_t *data, byte_t *type);
static size_t get_type_properties(byte_t primType, byte_t *type);
static int is_valid_identifier(const char *string);

static compile_error_t *handle_class_def(char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);
static compile_error_t *handle_field_def(char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);
static compile_error_t *handle_function_def(char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);

input_file_t *add_file(input_file_t *back, const char *filename)
{
	input_file_t *next = (input_file_t *)malloc(sizeof(input_file_t));
	if (!next)
		return NULL;
	if (back)
	{
		back->next = next;
		next->front = back->front;
	}
	else
		next->front = next;
	next->filename = filename;
	next->next = NULL;
	return next;
}

void free_file_list(input_file_t *front)
{
	input_file_t *curr = front;
	while (curr)
	{
		curr = front->next;
		free(front);
	}
}

compile_error_t *add_compile_error(compile_error_t *back, const char *file, int line, int type, const char *desc)
{
	compile_error_t *next = (compile_error_t *)malloc(sizeof(compile_error_t));
	if (!next)
		return NULL;
	if (back)
	{
		back->next = next;
		next->front = back->front;
	}
	else
		next->front = next;
	next->file = file;
	next->line = line;
	next->desc = desc;
	next->type = type;
	next->next = NULL;
	return next;
}

void free_compile_error_list(compile_error_t *front)
{
	compile_error_t *curr = front;
	while (curr)
	{
		curr = front->next;
		free(front);
	}
}

compile_error_t *compile(input_file_t *files, const char *outputDirectory)
{
	compile_error_t *errors = NULL;
	while (files)
	{
		errors = compile_file(files->filename, outputDirectory, errors);
		files = files->next;
	}
	return errors;
}

compile_error_t *compile_file(const char *file, const char *outputDirectory, compile_error_t *back)
{
	FILE *in;
	fopen_s(&in, file, "rb");
	if (!in)
		return add_compile_error(back, file, 0, error_error, "Failed to fopen for read");
	fseek(in, 0, SEEK_END);
	long length = ftell(in);
	fseek(in, 0, SEEK_SET);
	char *buf = (char *)malloc(length);
	if (!buf)
	{
		fclose(in);
		return add_compile_error(back, file, 0, error_error, "Failed to allocate buffer");
	}
	fread_s(buf, length, sizeof(char), length, in);
	fclose(in);

	size_t filenameSize = strlen(file) + 3 + 1; // + 3 for ".lb" extension and + 1 for null terminator
	char *nstr = (char *)malloc(filenameSize);
	if (!nstr)
	{
		free(buf);
		return add_compile_error(back, file, 0, error_error, "Failed to allocate buffer");
	}
	memcpy(nstr, file, filenameSize);
	char *lastSep = strrchr(nstr, '\\');
	if (!lastSep)
		lastSep = strrchr(nstr, '/');
	char *fname = lastSep ? lastSep : nstr;
	char *ext = strrchr(fname, '.');
	if (ext)
	{
		ext[1] = 'l';
		ext[2] = 'b';
		ext[3] = 0;
	}
	else
	{
		size_t len = strlen(fname);
		fname[len++] = '.';
		fname[len++] = 'l';
		fname[len++] = 'b';
		fname[len] = 0;
	}


	buffer_t *obuf = new_buffer(256);
	back = compile_data(buf, length, obuf, file, back);

	FILE *out;
	fopen_s(&out, nstr, "wb");
	if (!out)
	{
		free_buffer(obuf);
		free(nstr);
		return add_compile_error(back, file, 0, 0, "Failed to fopen for write", error_error);
	}
	fwrite(obuf->buf, sizeof(char), (size_t)(obuf->cursor - obuf->buf), out);

	fclose(out);
	free_buffer(obuf);
	free(nstr);

	return back ? back->front : NULL;
}

compile_error_t *compile_data(const char *data, size_t datalen, buffer_t *out, const char *srcFile, compile_error_t *back)
{
	line_t *formatted = format_document(data, datalen);

	line_t *curr = formatted;
	while (curr)
	{
		char *line = curr->line;
		if (*line != ';')
		{
			size_t tokenCount;
			char **tokens = tokenize_string(line, &tokenCount);
			byte_t cmd = get_command_byte(tokens[0]);
			switch (cmd)
			{
			case lb_class:
				back = handle_class_def(tokens, tokenCount, out, srcFile, curr->linenum, back);
				break;

			case lb_global:
				back = handle_field_def(tokens, tokenCount, out, srcFile, curr->linenum, back);
				break;
			case lb_function:
				back = handle_function_def(tokens, tokenCount, out, srcFile, curr->linenum, back);
				break;

			case lb_bool:
			case lb_char:
			case lb_uchar:
			case lb_short:
			case lb_ushort:
			case lb_int:
			case lb_uint:
			case lb_long:
			case lb_ulong:
			case lb_float:
			case lb_double:
			case lb_object:
			case lb_boolarray:
			case lb_chararray:
			case lb_uchararray:
			case lb_shortarray:
			case lb_ushortarray:
			case lb_intarray:
			case lb_uintarray:
			case lb_longarray:
			case lb_ulongarray:
			case lb_floatarray:
			case lb_doublearray:
			case lb_objectarray:
				if (tokenCount == 2)
				{
					out = put_byte(out, cmd);
					out = put_string(out, tokens[1]);
				}
				else if (tokenCount == 1)
				{
					back = add_compile_error(back, srcFile, curr->linenum, error_error, "Missing variable name declaration");
				}
				else
				{
					back = add_compile_error(back, srcFile, curr->linenum, error_warning, "Unecessary arguments following variable declaration");
				}
				break;

			case lb_setb:
				break;
			case lb_setw:
				break;
			case lb_setd:
				break;
			case lb_setq:
				break;
			case lb_setr4:
				break;
			case lb_setr8:
				break;
			case lb_seto:
				break;
			case lb_setv:
				break;
			case lb_setr:
				break;

			case lb_ret:
				break;
			case lb_retb:
				break;
			case lb_retw:
				break;
			case lb_retd:
				break;
			case lb_retq:
				break;
			case lb_retr4:
				break;
			case lb_retr8:
				break;
			case lb_reto:
				break;
			case lb_retv:
				break;
			case lb_retr:
				break;

			case lb_static_call:
				break;
			case lb_dynamic_call:
				break;

			case lb_add:
				break;
			case lb_sub:
				break;
			case lb_mul:
				break;
			case lb_div:
				break;
			case lb_mod:
				break;
			case lb_end:
				break;
			default:
				back = add_compile_error(back, srcFile, curr->linenum, error_error, "Unknown command");
				break;
			}

			free_tokenized_data(tokens, tokenCount);
		}

		curr = curr->next;
	}

	free_formatted(formatted);
	return back;
}

line_t *format_document(const char *data, size_t datalen)
{
	const char *cursor = data;
	const char *const end = data + datalen;
	line_t *front =  NULL;
	line_t *curr = front;

	buffer_t *linebuf = new_buffer(32);

	int foundLineStart = 0;
	int currline = 1;
	while (cursor < end)
	{
		switch (*cursor)
		{
		case '\t':
		case ' ':
			break;
		case '\r':
			cursor++;
		case '\n':
			if (foundLineStart)
			{
				foundLineStart = 0;

				put_char(linebuf, 0);			

				if (!curr)
				{
					front = curr = (line_t *)malloc(sizeof(line_t));
					if (!front)
					{
						free_buffer(linebuf);
						return NULL;
					}
				}
				else
				{
					curr->next = (line_t *)malloc(sizeof(line_t));
					if (!curr->next)
					{
						free_formatted(front);
						return NULL;
					}
					curr = curr->next;
				}
				curr->line = linebuf->buf;
				curr->linenum = currline;
				curr->next = NULL;

				free(linebuf);
				linebuf = new_buffer(32);
			}
			currline++;
			break;
		default:
			foundLineStart = 1;
			break;
		}

		if (foundLineStart)
			put_char(linebuf, *cursor);
		cursor++;
	}
	if (foundLineStart)
	{
		put_char(linebuf, 0);

		if (strlen(linebuf->buf) > 0)
		{

			if (!curr)
			{
				front = curr = (line_t *)malloc(sizeof(line_t));
				if (!front)
				{
					free_buffer(linebuf);
					return NULL;
				}
			}
			else
			{
				curr->next = (line_t *)malloc(sizeof(line_t));
				if (!curr->next)
				{
					free_formatted(front);
					return NULL;
				}
				curr = curr->next;
			}
			curr->line = linebuf->buf;
			curr->linenum = currline;
			curr->next = NULL;
			free(linebuf);
		}
		else
			free_buffer(linebuf);
	}
	else
		free(linebuf);


	return front;
}

void free_formatted(line_t *first)
{
	if (first)
	{
		free(first->line);
		free_formatted(first->next);
		free(first);
	}
}

byte_t get_command_byte(const char *string)
{
	if (!strcmp(string, "class"))
		return lb_class;
	else if (!strcmp(string, "global"))
		return lb_global;
	else if (!strcmp(string, "function"))
		return lb_function;

	else if (!strcmp(string, "bool"))
		return lb_bool;
	else if (!strcmp(string, "char"))
		return lb_char;
	else if (!strcmp(string, "uchar"))
		return lb_uchar;
	else if (!strcmp(string, "short"))
		return lb_short;
	else if (!strcmp(string, "ushort"))
		return lb_ushort;
	else if (!strcmp(string, "int"))
		return lb_int;
	else if (!strcmp(string, "uint"))
		return lb_uint;
	else if (!strcmp(string, "long"))
		return lb_long;
	else if (!strcmp(string, "ulong"))
		return lb_ulong;
	else if (!strcmp(string, "float"))
		return lb_float;
	else if (!strcmp(string, "double"))
		return lb_double;
	else if (!strcmp(string, "object"))
		return lb_object;
	else if (!strcmp(string, "boolarray"))
		return lb_boolarray;
	else if (!strcmp(string, "chararray"))
		return lb_chararray;
	else if (!strcmp(string, "uchararray"))
		return lb_chararray;
	else if (!strcmp(string, "shortarray"))
		return lb_shortarray;
	else if (!strcmp(string, "ushortarray"))
		return lb_ushortarray;
	else if (!strcmp(string, "intarray"))
		return lb_intarray;
	else if (!strcmp(string, "uintarray"))
		return lb_uintarray;
	else if (!strcmp(string, "longarray"))
		return lb_longarray;
	else if (!strcmp(string, "ulongarray"))
		return lb_ulongarray;
	else if (!strcmp(string, "floatarray"))
		return lb_floatarray;
	else if (!strcmp(string, "doublearray"))
		return lb_doublearray;
	else if (!strcmp(string, "objectarray"))
		return lb_objectarray;

	else if (!strcmp(string, "setb"))
		return lb_setb;
	else if (!strcmp(string, "setw"))
		return lb_setw;
	else if (!strcmp(string, "setd"))
		return lb_setd;
	else if (!strcmp(string, "setq"))
		return lb_setq;
	else if (!strcmp(string, "setr4"))
		return lb_setr4;
	else if (!strcmp(string, "setr8"))
		return lb_setr8;
	else if (!strcmp(string, "seto"))
		return lb_seto;
	else if (!strcmp(string, "setv"))
		return lb_setv;
	else if (!strcmp(string, "setr"))
		return lb_setr;

	else if (!strcmp(string, "ret"))
		return lb_ret;
	else if (!strcmp(string, "retb"))
		return lb_retb;
	else if (!strcmp(string, "retw"))
		return lb_retw;
	else if (!strcmp(string, "retd"))
		return lb_retd;
	else if (!strcmp(string, "retq"))
		return lb_retq;
	else if (!strcmp(string, "retr4"))
		return lb_retr4;
	else if (!strcmp(string, "retr8"))
		return lb_retr8;
	else if (!strcmp(string, "reto"))
		return lb_reto;
	else if (!strcmp(string, "retv"))
		return lb_retv;
	else if (!strcmp(string, "retr"))
		return lb_retr;

	else if (!strcmp(string, "static_call"))
		return lb_static_call;
	else if (!strcmp(string, "dynamic_call"))
		return lb_dynamic_call;

	else if (!strcmp(string, "add"))
		return lb_add;
	else if (!strcmp(string, "sub"))
		return lb_sub;
	else if (!strcmp(string, "mul"))
		return lb_mul;
	else if (!strcmp(string, "div"))
		return lb_div;
	else if (!strcmp(string, "mod"))
		return lb_mod;

	return lb_noop;
}

char **tokenize_string(const char *string, size_t *tokenCount)
{
	buffer_t *list = new_buffer(32);
	*tokenCount = 0;

	buffer_t *currstring = new_buffer(32);
	while (*string)
	{
		char c = *string;
		switch (c)
		{
		case ' ':
			currstring = put_char(currstring, 0);
			list = put_ulong(list, (size_t)currstring->buf);
			(*tokenCount)++;

			free(currstring);
			currstring = new_buffer(32);
			break;
		default:
			currstring = put_char(currstring, c);
			break;
		}
		string++;
	}

	if (currstring->cursor > currstring->buf)
	{
		currstring = put_char(currstring, 0);
		list = put_ulong(list, (size_t)currstring->buf);
		(*tokenCount)++;
	}

	size_t buflen = list->cursor - list->buf;
	char **result = (char **)malloc(buflen);
	if (!result)
		return 0;
	memcpy(result, list->buf, buflen);
	
	// Free only the list structures, not their contents
	free(list);
	free(currstring);
	
	return result;
}

void free_tokenized_data(char **data, size_t tokenCount)
{
	if (data)
	{
		for (size_t i = 0; i < tokenCount; i++)
		{
			if (data[i])
				free(data[i]);
		}
		free(data);
	}
}

size_t evaluate_constant(const char *string, data_t *data, byte_t *type)
{
	char *mString = (char *)string;
	char *lBracket = strchr(mString, '[');
	if (!lBracket)
	{
		if (!strcmp(mString, "true"))
		{
			data->bvalue = 1;
			*type = lb_word;
			return 1;
		}
		else if (!strcmp(mString, "false"))
		{
			data->bvalue = 0;
			*type = lb_word;
			return 1;
		}
		else if (!strcmp(mString, "null"))
		{
			data->ovalue = NULL;
			*type = lb_qword;
			return 1;
		}

		return 0;
	}
	char *dataStart = lBracket + 1;
	char *rBracket = strrchr(dataStart, ']');
	if (!rBracket)
		return 0;
	*lBracket = 0;
	*rBracket = 0;
	int size;
	if (!strcmp(mString, "byte"))
	{
		*type = lb_byte;
		data->cvalue = (lchar)atoll(dataStart);
		size = 1;
	}
	else if (!strcmp(mString, "word"))
	{
		*type = lb_word;
		data->svalue = (lshort)atoll(dataStart);
		size = 2;
	}
	else if (!strcmp(mString, "dword"))
	{
		*type = lb_dword;
		data->ivalue = (lint)atoll(dataStart);
		size = 4;
	}
	else if (!strcmp(mString, "qword"))
	{
		*type = lb_qword;
		data->lvalue = (llong)atoll(dataStart);
		size = 8;
	}
	else if (!strcmp(mString, "real4"))
	{
		*type = lb_real4;
		data->fvalue = (lfloat)atof(dataStart);
		size = 4;
	}
	else if (!strcmp(mString, "real8"))
	{
		*type = lb_real8;
		data->dvalue = (ldouble)atof(dataStart);
		size = 8;
	}
	else
		return 0;

	*lBracket = '[';
	*rBracket = ']';
	return size;
}

size_t get_type_properties(byte_t primType, byte_t *type)
{
	switch (primType)
	{
	case lb_bool:
	case lb_char:
	case lb_uchar:
		*type = lb_byte;
		return 1;
		break;
	case lb_short:
	case lb_ushort:
		*type = lb_word;
		return 2;
		break;
	case lb_int:
	case lb_uint:
		*type = lb_dword;
		return 4;
		break;
	case lb_long:
	case lb_ulong:
	case lb_object:
	case lb_boolarray:
	case lb_chararray:
	case lb_uchararray:
	case lb_shortarray:
	case lb_ushortarray:
	case lb_intarray:
	case lb_uintarray:
	case lb_longarray:
	case lb_ulongarray:
	case lb_floatarray:
	case lb_doublearray:
	case lb_objectarray:
		*type = lb_qword;
		return 8;
		break;
	case lb_float:
		*type = lb_real4;
		return 4;
		break;
	case lb_double:
		*type = lb_real8;
		return 8;
		break;
	default:
		break;
	}
	*type = 0;
	return 0;
}

int is_valid_identifier(const char *string)
{
	return 0;
}

compile_error_t *handle_class_def(char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back)
{
	if (tokenCount < 2)
		return add_compile_error(back, srcFile, srcLine, error_error, "Expected token class name declaration");
	
	const char *classname = tokens[1];
	put_byte(out, lb_class);
	put_string(out, classname);
	if (tokenCount > 2)
	{
		if (strcmp(tokens[2], "extends"))
			return add_compile_error(back, srcFile, srcLine, error_error, "Unexpected token, \"extends\" expected.");

		if (tokenCount == 3)
			return add_compile_error(back, srcFile, srcLine, error_error, "Expected token superclass name declaration");
		else if (tokenCount == 4)
		{
			put_byte(out, lb_extends);
			put_string(out, tokens[3]);
		}
		else
			return add_compile_error(back, srcFile, srcLine, error_warning, "Uneceassary arguments following class declaration");
	}
	return back;
}

compile_error_t *handle_field_def(char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back)
{
	if (tokenCount < 5)
	{
		switch (tokenCount)
		{
		case 1:
			back = add_compile_error(back, srcFile, srcLine, error_error, "Expected global storage specifier");
			break;
		case 2:
			back = add_compile_error(back, srcFile, srcLine, error_error, "Expected global write permissions specifier");
			break;
		case 3:
			back = add_compile_error(back, srcFile, srcLine, error_error, "Expected global data type specifier");
			break;
		case 4:
			back = add_compile_error(back, srcFile, srcLine, error_error, "Expected global name declaration");
			break;
		}
		return back;
	}
	int isStatic, isVarying;
	byte_t dataType;
	const char *name;

	if (!strcmp(tokens[1], "static"))
		isStatic = 1;
	else if (!strcmp(tokens[1], "dynamic"))
		isStatic = 0;
	else
		return add_compile_error(back, srcFile, srcLine, error_error, "Unexpected token, expected \"static\" or \"dynamic\"");

	if (!strcmp(tokens[2], "varying"))
		isVarying = 1;
	else if (!strcmp(tokens[2], "const"))
		isVarying = 0;
	else
		return add_compile_error(back, srcFile, srcLine, error_error, "Unexpected token, expected \"varying\" or \"const\"");

	dataType = get_command_byte(tokens[3]);
	if (dataType < lb_char || dataType > lb_objectarray)
		return add_compile_error(back, srcFile, srcLine, error_error, "Invalid global data type specifier");

	name = tokens[4];

	if (isStatic && tokenCount < 6)
		return add_compile_error(back, srcFile, srcLine, error_error, "Global declared static must have an initializer");
	else if (!isStatic && tokenCount > 5)
		return add_compile_error(back, srcFile, srcLine, error_error, "Global declared dynamic cannot have an initializer");

	if (tokenCount > 5)
	{
		data_t data;
		byte_t type;
		int initSize = evaluate_constant(tokens[5], &data, &type);

		if (!initSize)
			return add_compile_error(back, srcFile, srcLine, error_error, "Initialization of globals to variables is not supported");

		byte_t neededType;
		size_t neededSize = get_type_properties(dataType, &neededType);

		if (neededSize != initSize)
			return add_compile_error(back, srcFile, srcLine, error_error, "Type size mismatch");

		if (neededType != type)
			back = add_compile_error(back, srcFile, srcLine, error_warning, "Value types do not match");

		put_byte(out, lb_global);
		put_string(out, name);
		put_byte(out, isStatic ? lb_static : lb_dynamic);
		put_byte(out, isVarying ? lb_varying : lb_const);
		put_byte(out, 0); put_byte(out, 0); put_byte(out, 0); put_byte(out, 0); put_byte(out, 0);
		put_byte(out, dataType);
		switch (neededType)
		{
		case lb_byte:
			put_byte(out, data.cvalue);
			break;
		case lb_word:
			put_short(out, data.svalue);
			break;
		case lb_dword:
			put_int(out, data.ivalue);
			break;
		case lb_qword:
			put_long(out, data.lvalue);
			break;
		case lb_real4:
			put_float(out, data.fvalue);
			break;
		case lb_real8:
			put_double(out, data.dvalue);
			break;
		default:
			// something went wrong!
			break;
		}

		if (tokenCount > 6)
			back = add_compile_error(back, srcFile, srcLine, error_warning, "Unecessary arguments following global declaration");
	}
	else
	{
		put_byte(out, lb_global);
		put_string(out, name);
		put_byte(out, isStatic ? lb_static : lb_dynamic);
		put_byte(out, isVarying ? lb_varying : lb_const);
		put_byte(out, 0); put_byte(out, 0); put_byte(out, 0); put_byte(out, 0); put_byte(out, 0);
		put_byte(out, dataType);
	}

	return back;
}

compile_error_t *handle_function_def(char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back)
{
	return back;
}
