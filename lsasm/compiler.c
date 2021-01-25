#include "compiler.h"

#include "buffer.h"
#include <internal/types.h>
#include <internal/datau.h>
#include <internal/lb.h>
#include <stdio.h>

#define SIG_STRING_CHAR ((char)0x01)

typedef struct line_s line_t;
struct line_s
{
	char *line;
	line_t *next;
	int linenum;
};

static compile_error_t *compile_file(const char *file, const char *outputFile, compile_error_t *back, unsigned int version);
static compile_error_t *compile_data(const char *data, size_t datalen, buffer_t *out, const char *srcFile, compile_error_t *back, unsigned int version);
static line_t *format_document(const char *data, size_t datalen);
static void free_formatted(line_t *first);
static byte_t get_command_byte(const char *string);
static char **tokenize_string(const char *string, size_t *tokenCount);
static void free_tokenized_data(const char *const *data, size_t tokenCount);
static size_t evaluate_constant(const char *string, data_t *data, byte_t *type, int *isAbsoluteType);
static size_t get_type_properties(byte_t primType, byte_t *type);
static size_t get_type_width(byte_t type);
static int is_valid_identifier(const char *string);
static char **tokenize_function(char **tokens, size_t tokenCount, size_t *newTokenCount);
static byte_t *derive_function_args(const char *functionSig, size_t *argc);
static void free_derived_args(byte_t *args);

static compile_error_t *handle_class_def(char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);
static compile_error_t *handle_field_def(char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);
static compile_error_t *handle_function_def(char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);
static compile_error_t *handle_set_cmd(byte_t cmd, char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);
static compile_error_t *handle_ret_cmd(byte_t cmd, char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);
static compile_error_t *handle_call_cmd(byte_t cmd, char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);
static compile_error_t *handle_math_cmd(byte_t cmd, char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);

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

compile_error_t *create_base_compile_error(msg_func_t messenger)
{
	compile_error_t *next = (compile_error_t *)malloc(sizeof(compile_error_t));
	if (!next)
		return NULL;
	next->file = NULL;
	next->line = 0;
	next->type = -1;
	next->next = NULL;
	next->front = next;
	next->messenger = messenger;
	next->desc[0] = 0;
	return next;
}

compile_error_t *add_compile_error(compile_error_t *back, const char *file, int line, int type, const char *format, ...)
{
	compile_error_t *next = (compile_error_t *)malloc(sizeof(compile_error_t));
	if (!next)
		return NULL;
	if (back)
	{
		back->next = next;
		next->front = back->front;
		next->messenger = back->messenger;
	}
	else
	{
		next->front = next;
		next->messenger = NULL;
	}
	next->file = file;
	next->line = line;
	next->type = type;
	next->next = NULL;

	size_t len;
	size_t bufsize = sizeof(next->desc);
	char *curr = next->desc;

	switch (type)
	{
	case error_error:
		sprintf_s(curr, bufsize, "[ERRO] ");
		break;
	case error_warning:
		sprintf_s(curr, bufsize, "[WARN] ");
		break;
	case error_info:
		sprintf_s(curr, bufsize, "[INFO] ");
		break;
	default:
		return next;
	}
	len = strlen(curr);
	curr += len;
	bufsize -= len;

	if (file)
	{
		sprintf_s(curr, bufsize, "%s.%d : ", file, line);
		len = strlen(curr);
		curr += len;
		bufsize -= len;
	}

	va_list ls;
	va_start(ls, format);
	va_arg(ls, const char *);
	vsprintf_s(curr, bufsize, format, ls);
	va_end(ls);

	if (next->messenger)
		next->messenger(next->desc);

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

compile_error_t *compile(input_file_t *files, const char *outputDirectory, unsigned int version, msg_func_t messenger)
{
	compile_error_t *errors = create_base_compile_error(messenger);

	if (files)
	{
		files = files->front;
		if (version != 1)
			return add_compile_error(errors, "", 0, error_error, "Unsupported compile standard.");

		while (files)
		{
			errors = compile_file(files->filename, outputDirectory, errors, version);
			files = files->next;
		}
	}
	return errors;
}

compile_error_t *compile_file(const char *file, const char *outputDirectory, compile_error_t *back, unsigned int version)
{
	FILE *in;

	back = add_compile_error(back, NULL, 0, error_info, "Build: %s", file);

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

	buffer_t *obuf = new_buffer(256);
	back = compile_data(buf, length, obuf, file, back, version);

	int hasWarnings = 0;
	if (back)
	{
		compile_error_t *curr = back->front;
		while (curr)
		{
			if (curr->type == error_error)
			{
				back = add_compile_error(back, NULL, 0, error_info, "%s failed to build with errors.", file);
				return back->front;
			}
			else if (curr->type == error_warning)
				hasWarnings = 1;
			curr = curr->next;
		}
	}

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

	FILE *out;
	fopen_s(&out, nstr, "wb");
	if (!out)
	{
		free_buffer(obuf);
		free(nstr);
		return add_compile_error(back, file, 0, 0, "Failed to fopen for write", error_error);
	}
	fputc(0, out);
	fwrite(&version, sizeof(unsigned int), 1, out);
	fwrite(obuf->buf, sizeof(char), (size_t)(obuf->cursor - obuf->buf), out);

	fclose(out);
	free_buffer(obuf);
	free(nstr);

	if (hasWarnings)
		back = add_compile_error(back, NULL, 0, error_info, "%s built with warnings.", file);
	else
		back = add_compile_error(back, NULL, 0, error_info, "%s successfully built.", file);

	return back ? back->front : NULL;
}

compile_error_t *compile_data(const char *data, size_t datalen, buffer_t *out, const char *srcFile, compile_error_t *back, unsigned int version)
{
	line_t *formatted = format_document(data, datalen);

	line_t *curr = formatted;
	while (curr)
	{
		char *line = curr->line;
		if (*line != '#')
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
			case lb_setw:
			case lb_setd:
			case lb_setq:
			case lb_setr4:
			case lb_setr8:
			case lb_seto:
			case lb_setv:
			case lb_setr:
				back = handle_set_cmd(cmd, tokens, tokenCount, out, srcFile, curr->linenum, back);
				break;

			case lb_ret:
			case lb_retb:
			case lb_retw:
			case lb_retd:
			case lb_retq:
			case lb_retr4:
			case lb_retr8:
			case lb_reto:
			case lb_retv:
			case lb_retr:
				back = handle_ret_cmd(cmd, tokens, tokenCount, out, srcFile, curr->linenum, back);
				break;

			case lb_static_call:
			case lb_dynamic_call:
				back = handle_call_cmd(cmd, tokens, tokenCount, out, srcFile, curr->linenum, back);
				break;

			case lb_add:
			case lb_sub:
			case lb_mul:
			case lb_div:
			case lb_mod:
				back = handle_math_cmd(cmd, tokens, tokenCount, out, srcFile, curr->linenum, back);
				break;
			case lb_end:
				break;
			default:
				back = add_compile_error(back, srcFile, curr->linenum, error_error, "Unknown command \"%s\"", tokens[0]);
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

	int inDoubleQuotes = 0;
	int inEscape = 0;
	while (*string)
	{
		char c = *string;
		switch (c)
		{
		case '\"':
			if (inEscape)
			{
				inEscape = 0;
				put_char(currstring, 0);
			}
			else
			{
				if (!inDoubleQuotes)
					put_char(currstring, SIG_STRING_CHAR);
				inDoubleQuotes = !inDoubleQuotes;
			}
			break;
		case '\\':
			if (inDoubleQuotes)
			{
				if (inEscape)
					put_char(currstring, 0);
				inEscape = !inEscape;
			}
			else
				put_char(currstring, c);
			break;
		case 'n':
			if (inDoubleQuotes)
			{
				if (inEscape)
					put_char(currstring, '\n');
				else
					put_char(currstring, 'n');
			}
			else
				put_char(currstring, 'n');
			break;
		case 'r':
			if (inDoubleQuotes)
			{
				if (inEscape)
					put_char(currstring, '\r');
				else
					put_char(currstring, 'r');
			}
			else
				put_char(currstring, 'r');
			break;
		case 't':
			if (inDoubleQuotes)
			{
				if (inEscape)
					put_char(currstring, '\t');
				else
					put_char(currstring, 't');
			}
			else
				put_char(currstring, 't');
			break;
		case '0':
			if (inDoubleQuotes)
			{
				if (inEscape)
					put_char(currstring, '\0');
				else
					put_char(currstring, '0');
			}
			else
				put_char(currstring, '0');
			break;
		case ' ':
			if (inDoubleQuotes)
			{
				put_char(currstring, ' ');
			}
			else
			{
				if (currstring->cursor > currstring->buf)
				{
					currstring = put_char(currstring, 0);
					list = put_ulong(list, (size_t)currstring->buf);
					(*tokenCount)++;

					free(currstring);
					currstring = new_buffer(32);
				}
			}
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

size_t evaluate_constant(const char *string, data_t *data, byte_t *type, int *isAbsoluteType)
{
	char *mString = (char *)string;
	char *lBracket = strchr(mString, '[');
	if (!lBracket)
	{
		if (!strcmp(mString, "true"))
		{
			*isAbsoluteType = 0;
			data->bvalue = 1;
			*type = lb_byte;
			return sizeof(byte_t);
		}
		else if (!strcmp(mString, "false"))
		{
			*isAbsoluteType = 0;
			data->bvalue = 0;
			*type = lb_byte;
			return sizeof(byte_t);
		}
		else if (!strcmp(mString, "null"))
		{
			*isAbsoluteType = 0;
			data->ovalue = NULL;
			*type = lb_qword;
			return sizeof(qword_t);
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
		*isAbsoluteType = 0;
		*type = lb_byte;
		data->cvalue = (lchar)atoll(dataStart);
		size = sizeof(byte_t);
	}
	else if (!strcmp(mString, "word"))
	{
		*isAbsoluteType = 0;
		*type = lb_word;
		data->svalue = (lshort)atoll(dataStart);
		size = sizeof(word_t);
	}
	else if (!strcmp(mString, "dword"))
	{
		*isAbsoluteType = 0;
		*type = lb_dword;
		data->ivalue = (lint)atoll(dataStart);
		size = sizeof(dword_t);
	}
	else if (!strcmp(mString, "qword"))
	{
		*isAbsoluteType = 0;
		*type = lb_qword;
		data->lvalue = (llong)atoll(dataStart);
		size = sizeof(qword_t);
	}
	else if (!strcmp(mString, "real4"))
	{
		*isAbsoluteType = 0;
		*type = lb_real4;
		data->fvalue = (lfloat)atof(dataStart);
		size = sizeof(real4_t);
	}
	else if (!strcmp(mString, "real8"))
	{
		*isAbsoluteType = 0;
		*type = lb_real8;
		data->dvalue = (ldouble)atof(dataStart);
		size = sizeof(real8_t);
	}
	else if (!strcmp(mString, "bool"))
	{
		if (!strcmp(dataStart, "true"))
		{
			data->cvalue = 1;
		}
		else if (!strcmp(dataStart, "false"))
		{
			data->cvalue = 0;
		}
		else
			return 0;
		*isAbsoluteType = 1;
		*type = lb_bool;
		size = sizeof(byte_t);
	}
	else if (!strcmp(mString, "char"))
	{
		*isAbsoluteType = 1;
		*type = lb_char;
		data->cvalue = (lchar)atoll(dataStart);
		size = sizeof(lchar);
	}
	else if (!strcmp(mString, "uchar"))
	{
		*isAbsoluteType = 1;
		*type = lb_uchar;
		data->ucvalue = (luchar)atoll(dataStart);
		size = sizeof(luchar);
	}
	else if (!strcmp(mString, "short"))
	{
		*isAbsoluteType = 1;
		*type = lb_short;
		data->svalue = (lshort)atoll(dataStart);
		size = sizeof(lshort);
	}
	else if (!strcmp(mString, "ushort"))
	{
		*isAbsoluteType = 1;
		*type = lb_ushort;
		data->usvalue = (lushort)atoll(dataStart);
		size = sizeof(lushort);
	}
	else if (!strcmp(mString, "int"))
	{
		*isAbsoluteType = 1;
		*type = lb_int;
		data->ivalue = (lushort)atoll(dataStart);
		size = sizeof(lint);
	}
	else if (!strcmp(mString, "uint"))
	{
		*isAbsoluteType = 1;
		*type = lb_uint;
		data->uivalue = (luint)atoll(dataStart);
		size = sizeof(luint);
	}
	else if (!strcmp(mString, "long"))
	{
		*isAbsoluteType = 1;
		*type = lb_long;
		data->lvalue = (llong)atoll(dataStart);
		size = sizeof(llong);
	}
	else if (!strcmp(mString, "ulong"))
	{
		*isAbsoluteType = 1;
		*type = lb_ulong;
		data->ulvalue = (lulong)atoll(dataStart);
		size = sizeof(lulong);
	}
	else if (!strcmp(mString, "float"))
	{
		*isAbsoluteType = 1;
		*type = lb_float;
		data->fvalue = (lfloat)atof(dataStart);
		size = sizeof(lfloat);
	}
	else if (!strcmp(mString, "double"))
	{
		*isAbsoluteType = 1;
		*type = lb_double;
		data->dvalue = (ldouble)atof(dataStart);
		size = sizeof(ldouble);
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

size_t get_type_width(byte_t type)
{
	switch (type)
	{
	case lb_byte:
		return sizeof(byte_t);
		break;
	case lb_word:
		return sizeof(word_t);
		break;
	case lb_dword:
		return sizeof(dword_t);
		break;
	case lb_qword:
		return sizeof(qword_t);
		break;
	case lb_real4:
		return sizeof(real4_t);
		break;
	case lb_real8:
		return sizeof(real8_t);
		break;
	default:
		return 0;
		break;
	}
}

int is_valid_identifier(const char *string)
{
	return 0;
}

char **tokenize_function(char **tokens, size_t tokenCount, size_t *newTokenCount)
{
	buffer_t *temp = new_buffer(64);
	for (size_t i = 0; i < tokenCount; i++)
	{
		put_string(temp, tokens[i]);
		temp->cursor--;
		*(temp->cursor) = ' ';
		temp->cursor++;
	}
	*(temp->cursor) = 0;

	char *cursor = temp->buf;
	while (cursor < temp->cursor)
	{
		switch (*cursor)
		{
		case '(':
		case ')':
		case ',':
			*cursor = ' ';
			break;
		default:
			break;
		}

		cursor++;
	}

	char **result = tokenize_string(temp->buf, newTokenCount);

	free_buffer(temp);

	return result;
}

byte_t *derive_function_args(const char *functionSig, size_t *argc)
{
	buffer_t *buf = new_buffer(16);
	*argc = 0;

	while (*functionSig && *functionSig != '(')
		functionSig++;

	if (!(*functionSig))
	{
		free_buffer(buf);
		return NULL;
	}

	int isArray = 0;
	while (*functionSig && *functionSig != ')')
	{
		switch (*functionSig)
		{
		case 'C':
			put_byte(buf, isArray ? lb_chararray : lb_char);
			break;
		case 'c':
			put_byte(buf, isArray ? lb_uchararray : lb_uchar);
			break;
		case 'S':
			put_byte(buf, isArray ? lb_shortarray : lb_short);
			break;
		case 's':
			put_byte(buf, isArray ? lb_ushortarray : lb_ushort);
			break;
		case 'I':
			put_byte(buf, isArray ? lb_intarray : lb_int);
			break;
		case 'i':
			put_byte(buf, isArray ? lb_uintarray : lb_uint);
			break;
		case 'Q':
			put_byte(buf, isArray ? lb_longarray : lb_long);
			break;
		case 'q':
			put_byte(buf, isArray ? lb_ulongarray : lb_ulong);
			break;
		case 'B':
			put_byte(buf, isArray ? lb_boolarray : lb_bool);
			break;
		case 'F':
			put_byte(buf, isArray ? lb_floatarray : lb_float);
			break;
		case 'D':
			put_byte(buf, isArray ? lb_doublearray : lb_double);
			break;
		case 'L':
			put_byte(buf, isArray ? lb_objectarray : lb_object);
			while (*functionSig && *functionSig != ';' && *functionSig != ')')
				functionSig++;
			if (!(*functionSig))
			{
				free_buffer(buf);
				return NULL;
			}
			else if (*functionSig == ')')
			{
				free_buffer(buf);
				return NULL;
			}
			break;
		case '[':
			isArray = 1;
			functionSig++;
			continue;
			break;
		default:
			free_buffer(buf);
			return NULL;
			break;
		}
		isArray = 0;
		functionSig++;
	}

	byte_t *result = (byte_t *)buf->buf;
	free(buf);
	return result;
}

void free_derived_args(byte_t *args)
{
	free(args);
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
			return add_compile_error(back, srcFile, srcLine, error_error, "Unexpected token \"%s\", \"extends\" expected.", tokens[2]);

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
		return add_compile_error(back, srcFile, srcLine, error_error, "Unexpected token \"%s\", expected \"static\" or \"dynamic\"", tokens[1]);

	if (!strcmp(tokens[2], "varying"))
		isVarying = 1;
	else if (!strcmp(tokens[2], "const"))
		isVarying = 0;
	else
		return add_compile_error(back, srcFile, srcLine, error_error, "Unexpected token \"%s\", expected \"varying\" or \"const\"", tokens[1]);

	dataType = get_command_byte(tokens[3]);
	if (dataType < lb_char || dataType > lb_objectarray)
		return add_compile_error(back, srcFile, srcLine, error_error, "Invalid global data type specifier \"%s\"", tokens[3]);

	name = tokens[4];

	if (isStatic && tokenCount < 6)
		return add_compile_error(back, srcFile, srcLine, error_error, "Global declared static must have an initializer");
	else if (!isStatic && tokenCount > 5)
		return add_compile_error(back, srcFile, srcLine, error_error, "Global declared dynamic cannot have an initializer");

	if (tokenCount > 5)
	{
		data_t data;
		byte_t type;
		int isAbsolute;
		int initSize = evaluate_constant(tokens[5], &data, &type, &isAbsolute);

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
	if (tokenCount < 4)
	{
		switch (tokenCount)
		{
		case 1:
			back = add_compile_error(back, srcFile, srcLine, error_error, "Expected function execution mode specifier");
			break;
		case 2:
			back = add_compile_error(back, srcFile, srcLine, error_error, "Expected function linkage mode specifier");
			break;
		case 3:
			back = add_compile_error(back, srcFile, srcLine, error_error, "Expected function name declaration");
			break;
		}
		return back;
	}

	int isStatic, isInterp;
	if (!strcmp(tokens[1], "static"))
		isStatic = 1;
	else if (!strcmp(tokens[1], "dynamic"))
		isStatic = 0;
	else
		return add_compile_error(back, srcFile, srcLine, error_error, "Unexpected token \"%s\", expected \"static\" or \"dynamic\"", tokens[1]);

	if (!strcmp(tokens[2], "interp"))
		isInterp = 1;
	else if (!strcmp(tokens[2], "native"))
		isInterp = 0;
	else
		return add_compile_error(back, srcFile, srcLine, error_error, "Unexpected token \"%s\", expected \"interp\" or \"native\"", tokens[2]);

	size_t functionTokenCount;
	char **functionData = tokenize_function(tokens + 3, tokenCount - 3, &functionTokenCount);
	buffer_t *build = new_buffer(64);

	const char *name = functionData[0];
	byte_t argCount = 0;

	for (size_t i = 1; i < functionTokenCount; i++, argCount++)
	{
		byte_t type = get_command_byte(functionData[i]);
		if (type < lb_char || type > lb_objectarray)
		{
			free_buffer(build);
			return add_compile_error(back, srcFile, srcLine, error_error, "Illegal argument type");
		}

		const char *argName;

		i++;
		if (type == lb_object || type == lb_objectarray)
		{
			if (i == functionTokenCount)
			{
				free_buffer(build);
				return add_compile_error(back, srcFile, srcLine, error_error, "Expected function argument object classname");
			}

			const char *classname = functionData[i];

			i++;
			if (i == functionTokenCount)
			{
				free_buffer(build);
				return add_compile_error(back, srcFile, srcLine, error_error, "Expected function argument name");
			}

			argName = functionData[i];

			put_byte(build, type);
			put_string(build, classname);
			put_string(build, argName);
		}
		else if (i == functionTokenCount)
		{
			free_buffer(build);
			return add_compile_error(back, srcFile, srcLine, error_error, "Expected function argument name");
		}
		else
		{
			argName = functionData[i];

			put_byte(build, type);
			put_string(build, argName);
		}
	}

	put_byte(out, lb_function);
	put_byte(out, isStatic ? lb_static : lb_dynamic);
	put_byte(out, isInterp ? lb_interp : lb_native);
	put_string(out, name);
	put_byte(out, argCount);
	put_buf(out, build);

	free_buffer(build);
	free_tokenized_data(functionData, functionTokenCount);

	return back;
}

compile_error_t *handle_set_cmd(byte_t cmd, char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back)
{
	if (tokenCount < 2)
		return add_compile_error(back, srcFile, srcLine, error_error, "Expected variable name");

	const char *varname = tokens[1];
	int myWidth;
	byte_t myType;
	data_t setData;
	byte_t setType;
	size_t setWidth;
	int setIsAbsolute;

	if (cmd != lb_setr && tokenCount < 3)
		return add_compile_error(back, srcFile, srcLine, error_error, "Expeceted value");

	switch (cmd)
	{
	case lb_setv:
		put_byte(out, lb_setv);
		put_string(out, varname);
		put_byte(out, lb_value);
		put_string(out, tokens[2]);
		break;
	case lb_seto:
		if (!strcmp(tokens[2], "new"))
		{
			if (tokenCount < 4)
				return add_compile_error(back, srcFile, srcLine, error_error, "Expected instantiated class name");
			else if (tokenCount < 5)
				return add_compile_error(back, srcFile, srcLine, error_error, "Expected constructor call");

			char *classname = tokens[3];
			char *constructorSig = tokens[4];

			buffer_t *argbuf = new_buffer(16);

			size_t argc;
			byte_t *sig = derive_function_args(constructorSig, &argc);

			for (size_t i = 0; i < argc; i++)
			{
				const char *argstr = tokens[4 + i];

				byte_t reqArgType;
				size_t reqArgSize = get_type_properties(sig[i], &reqArgType);

				data_t argData;
				byte_t argType;
				int argIsAbsolute;
				size_t argSize = evaluate_constant(argstr, &argData, &argType, &argIsAbsolute);

				if (argSize)
				{
					put_byte(argbuf, lb_value);
					put_string(argbuf, argstr);
				}
				else
				{
					if (reqArgSize != argSize)
					{
						free_buffer(argbuf);
						return add_compile_error(back, srcFile, srcLine, error_error, "Type size mismatch (got: %d, needs: %d)", (int)argSize, (int)reqArgSize);
					}

					if (reqArgType != argType)
						back = add_compile_error(back, srcFile, srcLine, error_warning, "Value types do not match");

					put_byte(argbuf, reqArgType);
					switch (reqArgType)
					{
					case lb_byte:
						put_char(argbuf, argData.cvalue);
						break;
					case lb_word:
						put_short(argbuf, argData.svalue);
						break;
					case lb_dword:
						put_short(argbuf, argData.ivalue);
						break;
					case lb_qword:
						put_long(argbuf, argData.lvalue);
						break;
					case lb_real4:
						put_float(argbuf, argData.fvalue);
						break;
					case lb_real8:
						put_double(argbuf, argData.dvalue);
						break;
					default:
						break;
					}
				}
			}

			size_t sigend = strlen(constructorSig) - 1;
			constructorSig[sigend] = 0;

			put_byte(out, lb_seto);
			put_string(out, varname);
			put_byte(out, lb_new);
			put_string(out, classname);
			put_string(out, constructorSig);
			put_buf(out, argbuf);

			constructorSig[sigend] = ')';

			free_derived_args(sig);

			free_buffer(argbuf);
		}
		else if (!strcmp(tokens[2], "null"))
		{
			put_byte(out, lb_seto);
			put_string(out, varname);
			put_byte(out, lb_null);
		}
		else if (tokens[2][0] == SIG_STRING_CHAR)
		{
			put_byte(out, lb_seto);
			put_string(out, varname);
			put_byte(out, lb_string);
			put_string(out, tokens[2] + 1);
		}
		else
		{
			put_byte(out, lb_seto);
			put_string(out, varname);
			put_byte(out, lb_value);
			put_string(out, tokens[2]);
		}
		//else
		//	return add_compile_error(back, srcFile, srcLine, error_error, "Unexpected token; expected \"new\" or \"null\"");
		break;
	case lb_setr:
		put_byte(out, lb_setr);
		put_string(out, varname);
		break;
	default:
		myType = cmd + (lb_byte - lb_setb);
		myWidth = get_type_width(myType);
		setWidth = evaluate_constant(tokens[2], &setData, &setType, &setIsAbsolute);

		if (myWidth != setWidth)
			return add_compile_error(back, srcFile, srcLine, error_error, "Type size mismatch (got: %d, needs: %d)", (int)setWidth, (int)myWidth);

		if (myType != setType)
			back = add_compile_error(back, srcFile, srcLine, error_warning, "Value types do not match");

		put_byte(out, cmd);
		put_string(out, varname);
		//put_byte(out, myType);

		switch (myType)
		{
		case lb_byte:
			put_char(out, setData.cvalue);
			break;
		case lb_word:
			put_short(out, setData.svalue);
			break;
		case lb_dword:
			put_int(out, setData.ivalue);
			break;
		case lb_qword:
			put_long(out, setData.lvalue);
			break;
		case lb_real4:
			put_float(out, setData.fvalue);
			break;
		case lb_real8:
			put_double(out, setData.dvalue);
			break;
		}

		break;
	}

	return back;
}

compile_error_t *handle_ret_cmd(byte_t cmd, char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back)
{
	size_t valueRetDesSize;
	byte_t valueRetDesType;
	switch (cmd)
	{
	case lb_ret:
	case lb_retr:
		put_byte(out, cmd);
		if (tokenCount > 1)
			back = add_compile_error(back, srcFile, srcLine, error_warning, "Unecessary arguments following function return");
		break;
	case lb_retv:
		if (tokenCount < 2)
			return add_compile_error(back, srcFile, srcLine, error_error, "Expected variable name");

		put_byte(out, lb_retv);
		put_string(out, tokens[1]);

		if (tokenCount > 2)
			back = add_compile_error(back, srcFile, srcLine, error_warning, "Unecessary arguments following function return");
		break;
	case lb_retb:
		valueRetDesSize = sizeof(byte_t);
		valueRetDesType = lb_byte;
		goto handleValueRet;
	case lb_retw:
		valueRetDesSize = sizeof(word_t);
		valueRetDesType = lb_word;
		goto handleValueRet;
	case lb_retd:
		valueRetDesSize = sizeof(dword_t);
		valueRetDesType = lb_dword;
		goto handleValueRet;
	case lb_retq:
		valueRetDesSize = sizeof(qword_t);
		valueRetDesType = lb_qword;
		goto handleValueRet;
	case lb_retr4:
		valueRetDesSize = sizeof(real4_t);
		valueRetDesType = lb_real4;
		goto handleValueRet;
	case lb_retr8:
		valueRetDesSize = sizeof(real8_t);
		valueRetDesType = lb_real8;

		handleValueRet:
		if (tokenCount < 2)
			return add_compile_error(back, srcFile, srcLine, error_error, "Expected constant");
		else
		{
			data_t retData;
			byte_t retType;
			int retIsAbsoluteType;
			size_t retSize = evaluate_constant(tokens[1], &retData, &retType, &retIsAbsoluteType);
			if (retIsAbsoluteType)
				return add_compile_error(back, srcFile, srcLine, error_error, "Absolute type specifier not supported on return statement");

			if (retSize != valueRetDesSize)
				return add_compile_error(back, srcFile, srcLine, error_error, "Type size mismatch (got: %d, needs: %d)", (int)retSize, (int)valueRetDesSize);

			if (retType != valueRetDesType)
				back = add_compile_error(back, srcFile, srcLine, error_warning, "Value types do not match");

			put_byte(out, cmd);
			switch (retType)
			{
			case lb_byte:
				put_char(out, retData.cvalue);
				break;
			case lb_word:
				put_short(out, retData.svalue);
				break;
			case lb_dword:
				put_int(out, retData.ivalue);
				break;
			case lb_qword:
				put_long(out, retData.lvalue);
				break;
			case lb_real4:
				put_float(out, retData.fvalue);
				break;
			case lb_real8:
				put_double(out, retData.dvalue);
				break;
			}
		}

		if (tokenCount > 2)
			back = add_compile_error(back, srcFile, srcLine, error_warning, "Unecessary arguments following function return");

		break;
	}
	return back;
}

compile_error_t *handle_call_cmd(byte_t cmd, char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back)
{
	if (tokenCount < 2)
		return add_compile_error(back, srcFile, srcLine, error_error, "Expected function name");

	char *functionName = tokens[1];

	buffer_t *argBuffer = new_buffer(32);
	
	for (size_t i = 2; i < tokenCount; i++)
	{
		data_t argData;
		byte_t argType;
		int isAbsoluteType;
		size_t argSize = evaluate_constant(tokens[i], &argData, &argType, &isAbsoluteType);

		if (argSize == 0)
		{
			put_byte(argBuffer, lb_value);
			put_string(argBuffer, tokens[i]);
			continue;
		}

		if (isAbsoluteType)
			get_type_properties(argType, &argType);

		put_byte(argBuffer, argType);
		switch (argType)
		{
		case lb_byte:
			put_char(argBuffer, argData.cvalue);
			break;
		case lb_word:
			put_short(argBuffer, argData.svalue);
			break;
		case lb_dword:
			put_int(argBuffer, argData.ivalue);
			break;
		case lb_qword:
			put_long(argBuffer, argData.lvalue);
			break;
		case lb_real4:
			put_float(argBuffer, argData.fvalue);
			break;
		case lb_real8:
			put_double(argBuffer, argData.dvalue);
			break;
		}
	}

	size_t length = strlen(functionName);
	functionName[length - 1] = 0;

	put_byte(out, cmd);
	put_string(out, functionName);
	put_buf(out, argBuffer);

	functionName[length - 1] = ')';

	free_buffer(argBuffer);

	return back;
}

compile_error_t *handle_math_cmd(byte_t cmd, char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back)
{
	if (tokenCount < 4)
	{
		switch (tokenCount)
		{
		case 1:
			back = add_compile_error(back, srcFile, srcLine, error_error, "Expected destination variable name");
			break;
		case 2:
			back = add_compile_error(back, srcFile, srcLine, error_error, "Expected source variable name");
			break;
		case 3:
			back = add_compile_error(back, srcFile, srcLine, error_error, "Expected operation argument");
			break;
		}
		return back;
	}

	const char *dstVar = tokens[1];
	const char *srcVar = tokens[2];

	data_t argData;
	byte_t argType;
	int argIsAbsolute;
	size_t argSize = evaluate_constant(tokens[3], &argData, &argType, &argIsAbsolute);

	if (!argIsAbsolute)
	{
		return add_compile_error(back, srcFile, srcLine, error_error, "Arithmetic operation requires absolute type");
	}

	put_byte(out, cmd);
	put_string(out, dstVar);
	put_string(out, srcVar);

	if (argSize == 0)
	{
		const char *srcVar2 = tokens[3];
		put_byte(out, lb_value);
		put_string(out, srcVar2);
	}
	else
	{
		put_byte(out, argType);

		byte_t type;
		get_type_properties(argType, &type);
		switch (type)
		{
		case lb_byte:
			put_char(out, argData.cvalue);
			break;
		case lb_word:
			put_short(out, argData.svalue);
			break;
		case lb_dword:
			put_int(out, argData.ivalue);
			break;
		case lb_qword:
			put_long(out, argData.lvalue);
			break;
		case lb_real4:
			put_float(out, argData.fvalue);
			break;
		case lb_real8:
			put_double(out, argData.dvalue);
			break;
		}
	}

	if (tokenCount > 4)
		back = add_compile_error(back, srcFile, srcLine, error_warning, "Unecessary arguments following arithmetic operation");

	return back;
}

