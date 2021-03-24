#include "compiler.h"

#include "buffer.h"
#include <internal/types.h>
#include <internal/datau.h>
#include <internal/lb.h>
#include <stdio.h>

#define SIG_STRING_CHAR ((char)0x01)
#define SIG_CHAR_CHAR ((char)0x02)

typedef struct line_s line_t;
struct line_s
{
	char *line;
	line_t *next;
	int linenum;
};

static compile_error_t *compile_file(const char *file, const char *outputFile, compile_error_t *back, unsigned int version, int debug, alignment_t alignment, input_file_t **outputFiles);
static compile_error_t *compile_data(const char *data, size_t datalen, buffer_t *out, const char *srcFile, compile_error_t *back, unsigned int version, int debug, alignment_t alignment, buffer_t *debugOut);
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
static byte_t get_comparator_byte(const char *comparatorString);
static byte_t get_primitive_type(const char *stringType);

static compile_error_t *handle_class_def(char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);
static compile_error_t *handle_field_def(char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);
static compile_error_t *handle_function_def(char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);
static compile_error_t *handle_set_cmd(byte_t cmd, char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);
static compile_error_t *handle_array_creation(char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);
static compile_error_t *handle_ret_cmd(byte_t cmd, char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);
static compile_error_t *handle_call_cmd(byte_t cmd, char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);
static compile_error_t *handle_math_cmd(byte_t cmd, char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);
static compile_error_t *handle_unary_math_cmd(byte_t cmd, char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);
static compile_error_t *handle_if_style_cmd(byte_t cmd, char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back);

compile_error_t *compile(input_file_t *files, const char *outputDirectory, unsigned int version, int debug,
	alignment_t alignment, msg_func_t messenger, input_file_t **outputFiles)
{
	compile_error_t *errors = create_base_compile_error(messenger);
	input_file_t *base = (input_file_t *)MALLOC(sizeof(input_file_t));
	if (!base)
		return add_compile_error(errors, "", 0, error_error, "Allocation failure.");
	base->next = NULL;
	base->front = base;
	
	input_file_t *back = base;

	if (files)
	{
		files = files->front;
		if (version != 1)
			return add_compile_error(errors, "", 0, error_error, "Unsupported compile standard.");

		while (files)
		{
			errors = compile_file(files->filename, outputDirectory, errors, version, debug, alignment, &back);
			files = files->next;
		}
	}

	input_file_t *curr = base->next;
	input_file_t *front = base->next;

	base->next = NULL;
	free_file_list(base, 0);

	while (curr)
	{
		curr->front = front;
		curr = curr->next;
	}

	*outputFiles = front;
	return errors ? errors->front : NULL;
}

compile_error_t *compile_file(const char *file, const char *outputDirectory, compile_error_t *back,
	unsigned int version, int debug, alignment_t alignment, input_file_t **outputFiles)
{
	FILE *in = NULL;

	back = add_compile_error(back, NULL, 0, error_info, "Build: %s", file);

	fopen_s(&in, file, "rb");
	if (!in)
		return add_compile_error(back, file, 0, error_error, "Failed to fopen for read");
	fseek(in, 0, SEEK_END);
	long length = ftell(in);
	fseek(in, 0, SEEK_SET);
	char *buf = (char *)MALLOC(length);
	if (!buf)
	{
		fclose(in);
		return add_compile_error(back, file, 0, error_error, "Failed to allocate buffer");
	}
	fread_s(buf, length, sizeof(char), length, in);
	fclose(in);

	buffer_t *obuf = NEW_BUFFER(256);
	buffer_t *dbuf = debug ? NEW_BUFFER(256) : NULL;
	back = compile_data(buf, length, obuf, file, back, version, debug, alignment, dbuf);

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

	size_t outputDirSize = strlen(outputDirectory);
	size_t fileSize = strlen(file);
	size_t fullnameSize = outputDirSize + 1 + fileSize + 3 + 1; // + 3 for ".lb" extension and + 1 for null terminator
	char *nstr = (char *)MALLOC(fullnameSize);
	char *nameoff = nstr + outputDirSize + 1;
	if (!nstr)
	{
		FREE(buf);
		return add_compile_error(back, file, 0, error_error, "Failed to allocate buffer");
	}
	MEMCPY(nstr, outputDirectory, strlen(outputDirectory));
	*(nameoff - 1) = '\\';
	MEMCPY(nameoff, file, fileSize + 1);
	char *lastSep = strrchr(nameoff, '\\');
	if (!lastSep)
		lastSep = strrchr(nameoff, '\\');
	char *fname = lastSep ? lastSep : nameoff;
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

	*outputFiles = add_file(*outputFiles, nstr);

	FILE *out = NULL;
	fopen_s(&out, nstr, "wb");
	if (!out)
	{
		FREE_BUFFER(obuf);
		//free(nstr);
		return add_compile_error(back, file, 0, 0, "Failed to fopen for write", error_error);
	}
	fputc(0, out);
	fwrite(&version, sizeof(unsigned int), 1, out);
	fwrite(obuf->buf, sizeof(char), (size_t)(obuf->cursor - obuf->buf), out);

	fclose(out);
	FREE_BUFFER(obuf);

	if (debug)
	{
		fullnameSize++;
		nstr = (char *)MALLOC(fullnameSize);
		nameoff = nstr + outputDirSize + 1;
		if (!nstr)
		{
			FREE(buf);
			return add_compile_error(back, file, 0, error_error, "Failed to allocate buffer");
		}
		MEMCPY(nstr, outputDirectory, strlen(outputDirectory));
		*(nameoff - 1) = '\\';
		MEMCPY(nameoff, file, fileSize + 1);
		lastSep = strrchr(nstr, '\\');
		if (!lastSep)
			lastSep = strrchr(nstr, '/');
		fname = lastSep ? lastSep : nstr;
		ext = strrchr(fname, '.');
		if (ext)
		{
			ext[1] = 'l';
			ext[2] = 'd';
			ext[3] = 's';
			ext[4] = 0;
		}
		else
		{
			size_t len = strlen(fname);
			fname[len++] = '.';
			fname[len++] = 'l';
			fname[len++] = 'd';
			fname[len++] = 's';
			fname[len] = 0;
		}

		FILE *dfout;
		fopen_s(&dfout, nstr, "wb");
		if (dfout)
		{
			fwrite(&version, sizeof(unsigned int), 1, dfout);
			fputs(file, dfout);
			fputc(0, dfout);
			fwrite(dbuf->buf, sizeof(char), (size_t)(dbuf->cursor - dbuf->buf), dfout);
			fclose(dfout);
		}
		else
			return add_compile_error(back, file, 0, error_warning, "Failed to fopen for write debug information");
		FREE(nstr);
		FREE_BUFFER(dbuf);
	}

	if (hasWarnings)
		back = add_compile_error(back, NULL, 0, error_info, "%s built with warnings.", file);
	else
		back = add_compile_error(back, NULL, 0, error_info, "%s successfully built.", file);

	FREE(buf);
	return back ? back->front : NULL;
}

compile_error_t *compile_data(const char *data, size_t datalen, buffer_t *out, const char *srcFile,
	compile_error_t *back, unsigned int version, int debug, alignment_t alignment, buffer_t *debugOut)
{
	line_t *formatted = format_document(data, datalen);
	size_t alignAmount;

	line_t *curr = formatted;
	while (curr)
	{
		char *line = curr->line;
		if (*line != '#')
		{
			if (debug)
			{
				unsigned int off = (unsigned int)(out->cursor - out->buf);
				PUT_UINT(debugOut, off);
				PUT_INT(debugOut, curr->linenum);
			}

			size_t tokenCount;
			char **tokens = tokenize_string(line, &tokenCount);
			byte_t cmd = get_command_byte(tokens[0]);
			switch (cmd)
			{
			case lb_class:
				back = handle_class_def(tokens, tokenCount, out, srcFile, curr->linenum, back);
				break;

			case lb_global:
				alignAmount = alignment.globalAlignment - ((size_t)(out->cursor - out->buf) % (size_t)alignment.globalAlignment);
				out = PUT_BYTES(out, lb_align, alignAmount);
				back = handle_field_def(tokens, tokenCount, out, srcFile, curr->linenum, back);
				break;
			case lb_function:
				alignAmount = alignment.functionAlignment - ((size_t)(out->cursor - out->buf) % (size_t)alignment.functionAlignment);
				out = PUT_BYTES(out, lb_align, alignAmount);
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
					out = PUT_BYTE(out, cmd);
					out = PUT_STRING(out, tokens[1]);
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
			case lb_and:
			case lb_or:
			case lb_xor:
			case lb_lsh:
			case lb_rsh:
				back = handle_math_cmd(cmd, tokens, tokenCount, out, srcFile, curr->linenum, back);
				break;

			case lb_neg:
			case lb_not:
				back = handle_unary_math_cmd(cmd, tokens, tokenCount, out, srcFile, curr->linenum, back);
				break;

			case lb_if:
			case lb_while:
				back = handle_if_style_cmd(cmd, tokens, tokenCount, out, srcFile, curr->linenum, back);
				break;
			case lb_else:
				out = PUT_BYTE(out, lb_else);
				out = PUT_LONG(out, -1);
				//out = put_byte(out, tokenCount > 1);
				if (tokenCount > 1)
					back = handle_if_style_cmd(lb_if, tokens + 1, tokenCount - 1, out, srcFile, curr->linenum, back);
				break;
			case lb_end:
				out = PUT_BYTE(out, cmd);
				out = PUT_LONG(out, -1);
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

	buffer_t *linebuf = NEW_BUFFER(32);

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

				PUT_CHAR(linebuf, 0);			

				if (!curr)
				{
					front = curr = (line_t *)MALLOC(sizeof(line_t));
					if (!front)
					{
						FREE_BUFFER(linebuf);
						return NULL;
					}
				}
				else
				{
					curr->next = (line_t *)MALLOC(sizeof(line_t));
					if (!curr->next)
					{
						free_formatted(front);
						return NULL;
					}
					curr = curr->next;
				}
				curr->line = (char *)MALLOC((size_t)(linebuf->cursor - linebuf->buf));
				if (!curr->line)
				{
					FREE_BUFFER(linebuf);
					return NULL;
				}
				MEMCPY(curr->line, linebuf->buf, (size_t)(linebuf->cursor - linebuf->buf));
				curr->linenum = currline;
				curr->next = NULL;

				FREE_BUFFER(linebuf);
				linebuf = NEW_BUFFER(32);
			}
			currline++;
			break;
		default:
			foundLineStart = 1;
			break;
		}

		if (foundLineStart)
			PUT_CHAR(linebuf, *cursor);
		cursor++;
	}
	if (foundLineStart)
	{
		PUT_CHAR(linebuf, 0);

		if (strlen(linebuf->buf) > 0)
		{

			if (!curr)
			{
				front = curr = (line_t *)MALLOC(sizeof(line_t));
				if (!front)
				{
					FREE_BUFFER(linebuf);
					return NULL;
				}
			}
			else
			{
				curr->next = (line_t *)MALLOC(sizeof(line_t));
				if (!curr->next)
				{
					free_formatted(front);
					return NULL;
				}
				curr = curr->next;
			}
			curr->line = (char *)MALLOC((size_t)(linebuf->cursor - linebuf->buf));
			if (!curr->line)
			{
				FREE_BUFFER(linebuf);
				return NULL;
			}
			MEMCPY(curr->line, linebuf->buf, (size_t)(linebuf->cursor - linebuf->buf));
			curr->linenum = currline;
			curr->next = NULL;

			FREE_BUFFER(linebuf);
			linebuf = NEW_BUFFER(32);
		}
		else
		{
			FREE_BUFFER(linebuf);
			linebuf = NULL;
		}
	}
	else
	{
		FREE_BUFFER(linebuf);
		linebuf = NULL;
	}

	if (linebuf)
		FREE_BUFFER(linebuf);

	return front;
}

void free_formatted(line_t *first)
{
	if (first)
	{
		FREE(first->line);
		free_formatted(first->next);
		FREE(first);
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
	else if (!strcmp(string, "and"))
		return lb_and;
	else if (!strcmp(string, "or"))
		return lb_or;
	else if (!strcmp(string, "xor"))
		return lb_xor;
	else if (!strcmp(string, "lsh"))
		return lb_lsh;
	else if (!strcmp(string, "rsh"))
		return lb_rsh;

	else if (!strcmp(string, "neg"))
		return lb_neg;
	else if (!strcmp(string, "not"))
		return lb_not;

	else if (!strcmp(string, "if"))
		return lb_if;
	else if (!strcmp(string, "elif"))
		return lb_elif;
	else if (!strcmp(string, "else"))
		return lb_else;
	else if (!strcmp(string, "while"))
		return lb_while;
	else if (!strcmp(string, "end"))
		return lb_end;

	return lb_noop;
}

char **tokenize_string(const char *string, size_t *tokenCount)
{
	buffer_t *list = NEW_BUFFER(32);
	*tokenCount = 0;

	buffer_t *currstring = NEW_BUFFER(32);

	int inDoubleQuotes = 0;
	int inSingleQuotes = 0;
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
				PUT_CHAR(currstring, '\"');
			}
			else
			{
				if (!inDoubleQuotes)
					PUT_CHAR(currstring, SIG_STRING_CHAR);
				inDoubleQuotes = !inDoubleQuotes;
			}
			break;
		case '\'':
			if (inEscape)
			{
				inEscape = 0;
				PUT_CHAR(currstring, '\'');
			}
			else
			{
				if (!inSingleQuotes)
					PUT_CHAR(currstring, SIG_CHAR_CHAR);
				inSingleQuotes = !inSingleQuotes;
			}
			break;
		case '\\':
			if (inDoubleQuotes)
			{
				if (inEscape)
					PUT_CHAR(currstring, '\\');
				inEscape = !inEscape;
			}
			else
				PUT_CHAR(currstring, c);
			break;
		case 'n':
			if (inDoubleQuotes)
			{
				if (inEscape)
					PUT_CHAR(currstring, '\n');
				else
					PUT_CHAR(currstring, 'n');
			}
			else
				PUT_CHAR(currstring, 'n');
			break;
		case 'r':
			if (inDoubleQuotes)
			{
				if (inEscape)
					PUT_CHAR(currstring, '\r');
				else
					PUT_CHAR(currstring, 'r');
			}
			else
				PUT_CHAR(currstring, 'r');
			break;
		case 't':
			if (inDoubleQuotes)
			{
				if (inEscape)
					PUT_CHAR(currstring, '\t');
				else
					PUT_CHAR(currstring, 't');
			}
			else
				PUT_CHAR(currstring, 't');
			break;
		case '0':
			if (inDoubleQuotes)
			{
				if (inEscape)
					PUT_CHAR(currstring, '\0');
				else
					PUT_CHAR(currstring, '0');
			}
			else
				PUT_CHAR(currstring, '0');
			break;
		case ' ':
			if (inDoubleQuotes || inSingleQuotes)
			{
				PUT_CHAR(currstring, ' ');
			}
			else
			{
				if (currstring->cursor > currstring->buf)
				{
					currstring = PUT_CHAR(currstring, 0);
					char *curbuf = (char *)MALLOC((size_t)(currstring->cursor - currstring->buf));
					if (!curbuf)
						return NULL;
					MEMCPY(curbuf, currstring->buf, (size_t)(currstring->cursor - currstring->buf));


					list = PUT_ULONG(list, (size_t)curbuf);
					(*tokenCount)++;

					FREE_BUFFER(currstring);
					currstring = NEW_BUFFER(32);
				}
			}
			break;
		default:
			currstring = PUT_CHAR(currstring, c);
			break;
		}
		string++;
	}

	if (currstring->cursor > currstring->buf)
	{
		currstring = PUT_CHAR(currstring, 0);
		char *curbuf = (char *)MALLOC((size_t)(currstring->cursor - currstring->buf));
		if (!curbuf)
			return NULL;
		MEMCPY(curbuf, currstring->buf, (size_t)(currstring->cursor - currstring->buf));


		list = PUT_ULONG(list, (size_t)curbuf);
		(*tokenCount)++;
	}

	size_t buflen = list->cursor - list->buf;
	char **result = (char **)MALLOC(buflen);
	if (!result)
		return 0;
	MEMCPY(result, list->buf, buflen);
	
	// Free only the list structures, not their contents
	//free(list);
	//free(currstring);
	FREE_BUFFER(list);
	FREE_BUFFER(currstring);
	
	return result;
}

void free_tokenized_data(const char *const *data, size_t tokenCount)
{
	if (data)
	{
		for (size_t i = 0; i < tokenCount; i++)
		{
			if (data[i])
				FREE((void *)data[i]);
		}
		FREE((void *)data);
	}
}

size_t evaluate_constant(const char *string, data_t *data, byte_t *type, int *isAbsoluteType)
{
	char *mString = (char *)string;
	char *lBracket = strchr(mString, '[');
	if (!lBracket)
	{
		*isAbsoluteType = 0;
		if (!strcmp(mString, "true"))
		{
			data->bvalue = 1;
			*type = lb_byte;
			return sizeof(byte_t);
		}
		else if (!strcmp(mString, "false"))
		{
			data->bvalue = 0;
			*type = lb_byte;
			return sizeof(byte_t);
		}
		else if (!strcmp(mString, "null"))
		{
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
	{
		*lBracket = '[';
		*rBracket = ']';
		return 0;
	}

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
	buffer_t *temp = NEW_BUFFER(64);
	for (size_t i = 0; i < tokenCount; i++)
	{
		PUT_STRING(temp, tokens[i]);
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

	FREE_BUFFER(temp);

	return result;
}

byte_t *derive_function_args(const char *functionSig, size_t *argc)
{
	buffer_t *buf = NEW_BUFFER(16);
	*argc = 0;

	while (*functionSig && *functionSig != '(')
	{
		functionSig++;
	}
	functionSig++;

	if (!(*functionSig))
	{
		FREE_BUFFER(buf);
		return NULL;
	}

	int isArray = 0;
	while (*functionSig && *functionSig != ')')
	{
		if (argc)
			(*argc)++;
		switch (*functionSig)
		{
		case 'C':
			PUT_BYTE(buf, isArray ? lb_chararray : lb_char);
			break;
		case 'c':
			PUT_BYTE(buf, isArray ? lb_uchararray : lb_uchar);
			break;
		case 'S':
			PUT_BYTE(buf, isArray ? lb_shortarray : lb_short);
			break;
		case 's':
			PUT_BYTE(buf, isArray ? lb_ushortarray : lb_ushort);
			break;
		case 'I':
			PUT_BYTE(buf, isArray ? lb_intarray : lb_int);
			break;
		case 'i':
			PUT_BYTE(buf, isArray ? lb_uintarray : lb_uint);
			break;
		case 'Q':
			PUT_BYTE(buf, isArray ? lb_longarray : lb_long);
			break;
		case 'q':
			PUT_BYTE(buf, isArray ? lb_ulongarray : lb_ulong);
			break;
		case 'B':
			PUT_BYTE(buf, isArray ? lb_boolarray : lb_bool);
			break;
		case 'F':
			PUT_BYTE(buf, isArray ? lb_floatarray : lb_float);
			break;
		case 'D':
			PUT_BYTE(buf, isArray ? lb_doublearray : lb_double);
			break;
		case 'L':
			PUT_BYTE(buf, isArray ? lb_objectarray : lb_object);
			while (*functionSig && *functionSig != ';' && *functionSig != ')')
				functionSig++;
			if (!(*functionSig))
			{
				FREE_BUFFER(buf);
				return NULL;
			}
			else if (*functionSig == ')')
			{
				FREE_BUFFER(buf);
				return NULL;
			}
			break;
		case '[':
			isArray = 1;
			functionSig++;
			continue;
			break;
		default:
			FREE_BUFFER(buf);
			return NULL;
			break;
		}
		isArray = 0;
		functionSig++;
	}

	byte_t *result = (byte_t *)MALLOC((size_t)(buf->cursor - buf->buf));
	if (!result)
	{
		FREE_BUFFER(buf);
		return NULL;
	}
	MEMCPY(result, buf->buf, (size_t)(buf->cursor - buf->buf));
	FREE_BUFFER(buf);
		//(byte_t *)buf->buf;
//	free(buf);
	return result;
}

void free_derived_args(byte_t *args)
{
	FREE(args);
}

byte_t get_comparator_byte(const char *comparatorString)
{
	if (strlen(comparatorString) > 2)
		return 0;
	int c = (comparatorString[0] << 8) | (comparatorString[1]);

	switch (c)
	{
	case '==':
		return lb_equal;
		break;
	case '!=':
		return lb_nequal;
		break;
	case '<\0':
		return lb_less;
		break;
	case '<=':
		return lb_lequal;
		break;
	case '>\0':
		return lb_greater;
		break;
	case '>=':
		return lb_gequal;
		break;
	default:
		return 0;
	}
}

byte_t get_primitive_type(const char *stringType)
{
	if (!strcmp(stringType, "char"))
		return lb_char;
	else if (!strcmp(stringType, "uchar"))
		return lb_uchar;
	else if (!strcmp(stringType, "short"))
		return lb_short;
	else if (!strcmp(stringType, "ushort"))
		return lb_ushort;
	else if (!strcmp(stringType, "int"))
		return lb_int;
	else if (!strcmp(stringType, "luint"))
		return lb_uint;
	else if (!strcmp(stringType, "long"))
		return lb_long;
	else if (!strcmp(stringType, "ulong"))
		return lb_ulong;
	else if (!strcmp(stringType, "bool"))
		return lb_bool;
	else if (!strcmp(stringType, "float"))
		return lb_float;
	else if (!strcmp(stringType, "double"))
		return lb_double;
	else if (!strcmp(stringType, "object"))
		return lb_object;
	return 0;
}

compile_error_t *handle_class_def(char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back)
{
	if (tokenCount < 2)
		return add_compile_error(back, srcFile, srcLine, error_error, "Expected token class name declaration");
	
	const char *classname = tokens[1];
	PUT_BYTE(out, lb_class);
	PUT_STRING(out, classname);
	if (tokenCount > 2)
	{
		if (strcmp(tokens[2], "extends"))
			return add_compile_error(back, srcFile, srcLine, error_error, "Unexpected token \"%s\", \"extends\" expected.", tokens[2]);

		if (tokenCount == 3)
			return add_compile_error(back, srcFile, srcLine, error_error, "Expected token superclass name declaration");
		else if (tokenCount == 4)
		{
			if (strcmp(tokens[3], "null"))
			{
				PUT_BYTE(out, lb_extends);
				PUT_STRING(out, tokens[3]);
			}
		}
		else
			return add_compile_error(back, srcFile, srcLine, error_warning, "Uneceassary arguments following class declaration");
	}
	else
	{
		PUT_BYTE(out, lb_extends);
		PUT_STRING(out, "Object");
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
		int initSize = (int)evaluate_constant(tokens[5], &data, &type, &isAbsolute);

		if (!initSize)
			return add_compile_error(back, srcFile, srcLine, error_error, "Initialization of globals to variables is not supported");

		byte_t neededType;
		size_t neededSize = get_type_properties(dataType, &neededType);

		if (neededSize != initSize)
			return add_compile_error(back, srcFile, srcLine, error_error, "Type size mismatch");

		if (neededType != type)
			back = add_compile_error(back, srcFile, srcLine, error_warning, "Value types do not match");

		PUT_BYTE(out, lb_global);
		PUT_STRING(out, name);
		PUT_BYTE(out, isStatic ? lb_static : lb_dynamic);
		PUT_BYTE(out, isVarying ? lb_varying : lb_const);
		PUT_BYTE(out, 0); PUT_BYTE(out, 0); PUT_BYTE(out, 0); PUT_BYTE(out, 0); PUT_BYTE(out, 0);
		PUT_BYTE(out, dataType);
		switch (neededType)
		{
		case lb_byte:
			PUT_BYTE(out, data.cvalue);
			break;
		case lb_word:
			PUT_SHORT(out, data.svalue);
			break;
		case lb_dword:
			PUT_INT(out, data.ivalue);
			break;
		case lb_qword:
			PUT_LONG(out, data.lvalue);
			break;
		case lb_real4:
			PUT_FLOAT(out, data.fvalue);
			break;
		case lb_real8:
			PUT_DOUBLE(out, data.dvalue);
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
		PUT_BYTE(out, lb_global);
		PUT_STRING(out, name);
		PUT_BYTE(out, isStatic ? lb_static : lb_dynamic);
		PUT_BYTE(out, isVarying ? lb_varying : lb_const);
		PUT_BYTE(out, 0); PUT_BYTE(out, 0); PUT_BYTE(out, 0); PUT_BYTE(out, 0); PUT_BYTE(out, 0);
		PUT_BYTE(out, dataType);
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
	buffer_t *build = NEW_BUFFER(64);

	const char *name = functionData[0];
	byte_t argCount = 0;

	for (size_t i = 1; i < functionTokenCount; i++, argCount++)
	{
		byte_t type = get_command_byte(functionData[i]);
		if (type < lb_char || type > lb_objectarray)
		{
			FREE_BUFFER(build);
			return add_compile_error(back, srcFile, srcLine, error_error, "Illegal argument type");
		}

		const char *argName;

		i++;
		if (type == lb_object || type == lb_objectarray)
		{
			if (i == functionTokenCount)
			{
				FREE_BUFFER(build);
				return add_compile_error(back, srcFile, srcLine, error_error, "Expected function argument object classname");
			}

			const char *classname = functionData[i];

			i++;
			if (i == functionTokenCount)
			{
				FREE_BUFFER(build);
				return add_compile_error(back, srcFile, srcLine, error_error, "Expected function argument name");
			}

			argName = functionData[i];

			PUT_BYTE(build, type);
			PUT_STRING(build, classname);
			PUT_STRING(build, argName);
		}
		else if (i == functionTokenCount)
		{
			FREE_BUFFER(build);
			return add_compile_error(back, srcFile, srcLine, error_error, "Expected function argument name");
		}
		else
		{
			argName = functionData[i];

			PUT_BYTE(build, type);
			PUT_STRING(build, argName);
		}
	}

	PUT_BYTE(out, lb_function);
	PUT_BYTE(out, isStatic ? lb_static : lb_dynamic);
	PUT_BYTE(out, isInterp ? lb_interp : lb_native);
	PUT_STRING(out, name);
	PUT_BYTE(out, argCount);
	PUT_BUF(out, build);

	FREE_BUFFER(build);
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
		PUT_BYTE(out, lb_setv);
		PUT_STRING(out, varname);
		//put_byte(out, lb_value);
		PUT_STRING(out, tokens[2]);
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

			buffer_t *argbuf = NEW_BUFFER(16);

			size_t argc;
			byte_t *sig = derive_function_args(constructorSig, &argc);

			for (size_t i = 0, j = 5; i < argc; i++, j++)
			{
				const char *argstr = tokens[j];

				byte_t reqArgType;
				size_t reqArgSize = get_type_properties(sig[i], &reqArgType);

				data_t argData;
				byte_t argType;
				int argIsAbsolute;
				size_t argSize = evaluate_constant(argstr, &argData, &argType, &argIsAbsolute);

				if (argSize == 0)
				{
					if (tokens[j][0] == SIG_STRING_CHAR)
					{
						PUT_BYTE(argbuf, lb_string);
						PUT_STRING(argbuf, tokens[j] + 1);
					}
					else
					{
						PUT_BYTE(argbuf, lb_value);
						PUT_STRING(argbuf, tokens[j]);
					}
					continue;
				}
				else
				{
					if (reqArgSize != argSize)
					{
						FREE_BUFFER(argbuf);
						return add_compile_error(back, srcFile, srcLine, error_error, "Type size mismatch (got: %d, needs: %d)", (int)argSize, (int)reqArgSize);
					}

					if (reqArgType != argType)
						back = add_compile_error(back, srcFile, srcLine, error_warning, "Value types do not match");

					PUT_BYTE(argbuf, reqArgType);
					switch (reqArgType)
					{
					case lb_byte:
						PUT_CHAR(argbuf, argData.cvalue);
						break;
					case lb_word:
						PUT_SHORT(argbuf, argData.svalue);
						break;
					case lb_dword:
						PUT_INT(argbuf, argData.ivalue);
						break;
					case lb_qword:
						PUT_LONG(argbuf, argData.lvalue);
						break;
					case lb_real4:
						PUT_FLOAT(argbuf, argData.fvalue);
						break;
					case lb_real8:
						PUT_DOUBLE(argbuf, argData.dvalue);
						break;
					case lb_object:
						break;
					default:
						break;
					}
				}
			}

			size_t sigend = strlen(constructorSig) - 1;
			constructorSig[sigend] = 0;

			PUT_BYTE(out, lb_seto);
			PUT_STRING(out, varname);
			PUT_BYTE(out, lb_new);
			PUT_STRING(out, classname);
			PUT_STRING(out, constructorSig);
			PUT_BUF(out, argbuf);

			constructorSig[sigend] = ')';

			free_derived_args(sig);

			FREE_BUFFER(argbuf);
		}
		else if (!strcmp(tokens[2], "null"))
		{
			PUT_BYTE(out, lb_seto);
			PUT_STRING(out, varname);
			PUT_BYTE(out, lb_null);
		}
		else if (get_primitive_type(tokens[2]))
		{
			PUT_BYTE(out, lb_seto);
			PUT_STRING(out, varname);
			back = handle_array_creation(&tokens[2], tokenCount - 2, out, srcFile, srcLine, back);
		}
		else if (tokens[2][0] == SIG_STRING_CHAR)
		{
			PUT_BYTE(out, lb_seto);
			PUT_STRING(out, varname);
			PUT_BYTE(out, lb_string);
			PUT_STRING(out, tokens[2] + 1);
		}
		else
		{
			/*put_byte(out, lb_seto);
			put_string(out, varname);
			put_byte(out, lb_value);
			put_string(out, tokens[2]);*/
			return add_compile_error(back, srcFile, srcLine, error_error, "Invalid usage of seto; must be initialization of object, array, string, or null");
		}
		
		//else
		//	return add_compile_error(back, srcFile, srcLine, error_error, "Unexpected token; expected \"new\" or \"null\"");
		break;
	case lb_setr:
		PUT_BYTE(out, lb_setr);
		PUT_STRING(out, varname);
		break;
	default:
		myType = cmd + (lb_byte - lb_setb);
		myWidth = (int)get_type_width(myType);

		if (tokens[2][0] == SIG_CHAR_CHAR)
		{
			setData.cvalue = tokens[2][1];
			setWidth = sizeof(lchar);
			setType = lb_byte;
		}
		else
		{
			setWidth = evaluate_constant(tokens[2], &setData, &setType, &setIsAbsolute);
		}

		if (myWidth != setWidth)
			return add_compile_error(back, srcFile, srcLine, error_error, "Type size mismatch (got: %d, needs: %d)", (int)setWidth, (int)myWidth);

		if (myType != setType)
			back = add_compile_error(back, srcFile, srcLine, error_warning, "Value types do not match");

		PUT_BYTE(out, cmd);
		PUT_STRING(out, varname);
		//put_byte(out, myType);

		switch (myType)
		{
		case lb_byte:
			PUT_CHAR(out, setData.cvalue);
			break;
		case lb_word:
			PUT_SHORT(out, setData.svalue);
			break;
		case lb_dword:
			PUT_INT(out, setData.ivalue);
			break;
		case lb_qword:
			PUT_LONG(out, setData.lvalue);
			break;
		case lb_real4:
			PUT_FLOAT(out, setData.fvalue);
			break;
		case lb_real8:
			PUT_DOUBLE(out, setData.dvalue);
			break;
		}

		break;
	}

	return back;
}

compile_error_t *handle_array_creation(char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back)
{
	if (tokenCount == 0)
		return add_compile_error(back, srcFile, srcLine, error_error, "Not enough array initialization arguments");
	else if (tokenCount == 1)
		return add_compile_error(back, srcFile, srcLine, error_error, "Expected array length specifier");

	byte_t type = get_primitive_type(tokens[0]);

	data_t argData;
	byte_t argType;
	int argIsAbsolute;
	size_t argSize = evaluate_constant(tokens[1], &argData, &argType, &argIsAbsolute);
	if (argIsAbsolute)
		return add_compile_error(back, srcFile, srcLine, error_error, "Array initialization does not support absolute type");
	////else if (argSize == 0)
		//return add_compile_error(back, srcFile, srcLine, error_error, "Invalid array length specifier");
	switch (argSize)
	{
	case 0:
		PUT_BYTE(out, lb_value);
		PUT_STRING(out, tokens[1]);
		goto post_init_array;
		break;
	case lb_byte:
		argData.uivalue = (luint)argData.ucvalue;
		break;
	case lb_word:
		argData.uivalue = (luint)argData.usvalue;
		break;
	case lb_dword:
		break;
	case lb_qword:
		argData.uivalue = (luint)argData.ulvalue;
		back = add_compile_error(back, srcFile, srcLine, error_warning, "Arrays have maximum 32-bit unsigned length but requested length is 64-bit");
		break;
	case lb_real4:
	case lb_real8:
		return add_compile_error(back, srcFile, srcLine, error_error, "Array initialization requires an integral length type");
		break;
	}

	PUT_BYTE(out, type);
	PUT_UINT(out, argData.uivalue);

	post_init_array:
	if (tokenCount > 2)
	{
		back = add_compile_error(back, srcFile, srcLine, error_warning, "Unecessary arguments in array initialization");
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
		PUT_BYTE(out, cmd);
		if (tokenCount > 1)
			back = add_compile_error(back, srcFile, srcLine, error_warning, "Unecessary arguments following function return");
		break;
	case lb_retv:
		if (tokenCount < 2)
			return add_compile_error(back, srcFile, srcLine, error_error, "Expected variable name");

		PUT_BYTE(out, lb_retv);
		PUT_STRING(out, tokens[1]);

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

			PUT_BYTE(out, cmd);
			switch (retType)
			{
			case lb_byte:
				PUT_CHAR(out, retData.cvalue);
				break;
			case lb_word:
				PUT_SHORT(out, retData.svalue);
				break;
			case lb_dword:
				PUT_INT(out, retData.ivalue);
				break;
			case lb_qword:
				PUT_LONG(out, retData.lvalue);
				break;
			case lb_real4:
				PUT_FLOAT(out, retData.fvalue);
				break;
			case lb_real8:
				PUT_DOUBLE(out, retData.dvalue);
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

	buffer_t *argBuffer = NEW_BUFFER(32);
	
	for (size_t i = 2; i < tokenCount; i++)
	{
		data_t argData;
		byte_t argType;
		int isAbsoluteType;
		size_t argSize = evaluate_constant(tokens[i], &argData, &argType, &isAbsoluteType);

		if (argSize == 0)
		{
			if (tokens[i][0] == SIG_STRING_CHAR)
			{
				PUT_BYTE(argBuffer, lb_string);
				PUT_STRING(argBuffer, tokens[i] + 1);
			}
			else if (tokens[i][0] == SIG_CHAR_CHAR)
			{
				PUT_BYTE(argBuffer, lb_byte);
				PUT_BYTE(argBuffer, tokens[i][1]);
			}
			else
			{
				if (!strcmp(tokens[i], "ret"))
				{
					PUT_BYTE(argBuffer, lb_ret);
				}
				else
				{
					PUT_BYTE(argBuffer, lb_value);
					PUT_STRING(argBuffer, tokens[i]);
				}
			}
			continue;
		}

		if (isAbsoluteType)
			get_type_properties(argType, &argType);

		PUT_BYTE(argBuffer, argType);
		switch (argType)
		{
		case lb_byte:
			PUT_CHAR(argBuffer, argData.cvalue);
			break;
		case lb_word:
			PUT_SHORT(argBuffer, argData.svalue);
			break;
		case lb_dword:
			PUT_INT(argBuffer, argData.ivalue);
			break;
		case lb_qword:
			PUT_LONG(argBuffer, argData.lvalue);
			break;
		case lb_real4:
			PUT_FLOAT(argBuffer, argData.fvalue);
			break;
		case lb_real8:
			PUT_DOUBLE(argBuffer, argData.dvalue);
			break;
		}
	}

	size_t length = strlen(functionName);
	functionName[length - 1] = 0;

	PUT_BYTE(out, cmd);
	PUT_STRING(out, functionName);
	PUT_BUF(out, argBuffer);

	functionName[length - 1] = ')';

	FREE_BUFFER(argBuffer);

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
	size_t argSize;
	
	if (tokens[3][0] == SIG_STRING_CHAR)
	{
		return add_compile_error(back, srcFile, srcLine, error_error, "Operation requires integral or floating point type");
	}
	else if (tokens[3][0] == SIG_CHAR_CHAR)
	{
		argData.cvalue = tokens[3][1];
		argType = lb_char;
		argIsAbsolute = 1;
		argSize = sizeof(lchar);
	}
	else
	{
		argSize = evaluate_constant(tokens[3], &argData, &argType, &argIsAbsolute);

		if (argSize != 0 && !argIsAbsolute)
		{
			return add_compile_error(back, srcFile, srcLine, error_error, "Operation requires absolute type");
		}
	}

	PUT_BYTE(out, cmd);
	PUT_STRING(out, dstVar);
	PUT_STRING(out, srcVar);

	if (argSize == 0)
	{
		const char *srcVar2 = tokens[3];
		PUT_BYTE(out, lb_value);
		PUT_STRING(out, srcVar2);
	}
	else
	{
		PUT_BYTE(out, argType);

		byte_t type;
		get_type_properties(argType, &type);
		switch (type)
		{
		case lb_byte:
			PUT_CHAR(out, argData.cvalue);
			break;
		case lb_word:
			PUT_SHORT(out, argData.svalue);
			break;
		case lb_dword:
			PUT_INT(out, argData.ivalue);
			break;
		case lb_qword:
			PUT_LONG(out, argData.lvalue);
			break;
		case lb_real4:
			PUT_FLOAT(out, argData.fvalue);
			break;
		case lb_real8:
			PUT_DOUBLE(out, argData.dvalue);
			break;
		}
	}

	if (tokenCount > 4)
		back = add_compile_error(back, srcFile, srcLine, error_warning, "Unecessary arguments following operation");

	return back;
}

compile_error_t *handle_unary_math_cmd(byte_t cmd, char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back)
{
	if (tokenCount < 3)
	{
		switch (tokenCount)
		{
		case 1:
			back = add_compile_error(back, srcFile, srcLine, error_error, "Expected destination variable name");
			break;
		case 2:
			back = add_compile_error(back, srcFile, srcLine, error_error, "Expected source variable name");
			break;
		}
		return back;
	}

	const char *dstVar = tokens[1];
	const char *srcVar = tokens[2];

	PUT_BYTE(out, cmd);
	PUT_STRING(out, dstVar);
	PUT_STRING(out, srcVar);

	if (tokenCount > 3)
		back = add_compile_error(back, srcFile, srcLine, error_warning, "Unecessary arguments following unary operation");

	return back;
}

compile_error_t *handle_if_style_cmd(byte_t cmd, char **tokens, size_t tokenCount, buffer_t *out, const char *srcFile, int srcLine, compile_error_t *back)
{
	if (tokenCount < 2)
		return add_compile_error(back, srcFile, srcLine, error_error, "Expected variable name");

	buffer_t *temp = NEW_BUFFER(32);
	byte_t count = lb_one;

	data_t lhsData;
	byte_t lhsType;
	int lhsIsAbsolute;
	size_t lhsSize = evaluate_constant(tokens[1], &lhsData, &lhsType, &lhsIsAbsolute);

	if (lhsSize == 0)
	{
		PUT_BYTE(temp, lb_value);
		PUT_STRING(temp, tokens[1]);
	}
	else if (!lhsIsAbsolute)
	{
		FREE_BUFFER(temp);
		return add_compile_error(back, srcFile, srcLine, error_error, "Compare operation requires absolute type");
	}
	else
	{
		PUT_BYTE(temp, lhsType);
		switch (lhsSize)
		{
		case 1:
			PUT_CHAR(temp, lhsData.cvalue);
			break;
		case 2:
			PUT_SHORT(temp, lhsData.svalue);
			break;
		case 4:
			PUT_INT(temp, lhsData.ivalue);
			break;
		case 8:
			PUT_LONG(temp, lhsData.lvalue);
			break;
		}
	}

	if (tokenCount > 2)
	{
		count = lb_two;
		byte_t comparatorByte = get_comparator_byte(tokens[2]);
		if (comparatorByte == 0)
		{
			FREE_BUFFER(temp);
			return add_compile_error(back, srcFile, srcLine, error_error, "Unexpected token \"%s\", expected comparator", tokens[2]);
		}
		PUT_BYTE(temp, comparatorByte);

		if (tokenCount < 4)
		{
			FREE_BUFFER(temp);
			return add_compile_error(back, srcFile, srcLine, error_error, "Expected compare argument");
		}

		data_t rhsData;
		byte_t rhsType;
		int rhsIsAbsolute;
		size_t rhsSize = evaluate_constant(tokens[3], &rhsData, &rhsType, &rhsIsAbsolute);

		if (rhsSize == 0)
		{
			PUT_BYTE(temp, lb_value);
			PUT_STRING(temp, tokens[3]);
		}
		else if (!rhsIsAbsolute)
		{
			FREE_BUFFER(temp);
			return add_compile_error(back, srcFile, srcLine, error_error, "Compare operation requires absolute type");
		}
		else
		{
			PUT_BYTE(temp, rhsType);
			switch (rhsSize)
			{
			case 1:
				PUT_CHAR(temp, rhsData.cvalue);
				break;
			case 2:
				PUT_SHORT(temp, rhsData.svalue);
				break;
			case 4:
				PUT_INT(temp, rhsData.ivalue);
				break;
			case 8:
				PUT_LONG(temp, rhsData.lvalue);
				break;
			}
		}
	}

	PUT_BYTE(out, cmd);
	PUT_BYTE(out, count);
	PUT_BUF(out, temp);
	PUT_LONG(out, -1);
	FREE_BUFFER(temp);

	return back;
}
