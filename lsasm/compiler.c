#include "compiler.h"

#include "buffer.h"
#include <internal/types.h>
#include <internal/datau.h>
#include <internal/lb.h>
#include <stdio.h>
#include <assert.h>

#include "file_util.h"

#define SIG_STRING_CHAR ((char)0x01)
#define SIG_CHAR_CHAR ((char)0x02)

enum
{
	constant_not_absolute = 0,
	constant_absolute = 1,
	constant_both = 2
};

typedef struct line_s line_t;
struct line_s
{
	char *line;
	line_t *next;
	int linenum;
};

typedef struct file_compile_options_s file_compile_options_t;
struct file_compile_options_s
{
	const char *unitname;
	const char *inFilePath;
	const char *outputDirectory;
	compile_error_t *back;
	unsigned int version;
	int debug;
	alignment_t alignment;
	input_file_t **outputFiles;
	char **classpaths;
};

typedef struct data_compile_options_s data_compile_options_t;
struct data_compile_options_s
{
	const char *data;
	size_t datalen;
	buffer_t *out;
	const char *srcFile;
	compile_error_t *back;
	unsigned int version;
	int debug;
	alignment_t alignment;
	buffer_t *debugOut;
	char **classpaths;
};

typedef struct compile_state_s compile_state_t;
struct compile_state_s
{
	byte_t cmd;
	char **tokens;
	size_t tokencount;
	buffer_t *out;
	char package[MAX_PATH];
	const char *srcfile;
	int srcline;
	compile_error_t *back;
	LSCUCONTEXT lscuctx;
};

static compile_error_t *compile_file(file_compile_options_t *options);
static compile_error_t *compile_data(data_compile_options_t *options);
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
static byte_t resolve_data_type(compile_state_t *state, char *declared, char *classname, size_t namelen);

static void handle_class_def(compile_state_t *state);
static void handle_field_def(compile_state_t *state);
static void handle_function_def(compile_state_t *state);
static void handle_constructor_def(compile_state_t *state);
static void handle_set_cmd(compile_state_t *state);
static void handle_cast_cmd(compile_state_t *state);
static void handle_array_creation(compile_state_t *state);
static void handle_ret_cmd(compile_state_t *state);
static void handle_call_cmd(compile_state_t *state);
static void handle_math_cmd(compile_state_t *state);
static void handle_unary_math_cmd(compile_state_t *state);
static void handle_if_style_cmd(compile_state_t *state);

compile_error_t *compile(compiler_options_t *options)
{
	compile_error_t *errors = create_base_compile_error(options->messenger);
	input_file_t *base = (input_file_t *)MALLOC(sizeof(input_file_t));
	if (!base)
		return add_compile_error(errors, "", 0, error_error, "Allocation failure.");
	base->next = NULL;
	base->front = base;
	
	input_file_t *back = base;

	if (options->inFiles)
	{
		options->inFiles = options->inFiles->front;
		if (options->version != 1)
			return add_compile_error(errors, "", 0, error_error, "Unsupported compile standard.");

		while (options->inFiles)
		{
			char *strs[LSCU_MAX_CLASSPATHS];
			for (int i = 0; i < LSCU_MAX_CLASSPATHS; i++)
				strs[i] = options->classpaths[i];

			file_compile_options_t fileOptions = {
				options->inFiles->unitname,
				options->inFiles->fullpath,
				options->outDirectory,
				errors,
				options->version,
				options->debug,
				options->alignment,
				&back,
				&strs
			};


			errors = compile_file(&fileOptions);
			options->inFiles = options->inFiles->next;
		}
	}

	input_file_t *curr = base->next;
	input_file_t *front = base->next;

	base->next = NULL;
	free_file_list(base);

	while (curr)
	{
		curr->front = front;
		curr = curr->next;
	}

	*options->outputFiles = front;
	return errors ? errors->front : NULL;
}

compile_error_t *compile_file(file_compile_options_t *options)
{
	FILE *in = NULL;

	options->back = add_compile_error(options->back, NULL, 0, error_info, "Build: %s", options->inFilePath);

	fopen_s(&in, options->inFilePath, "rb");
	if (!in)
		return add_compile_error(options->back, options->inFilePath, 0, error_error, "Failed to fopen for read");
	fseek(in, 0, SEEK_END);
	long length = ftell(in);
	fseek(in, 0, SEEK_SET);
	char *buf = (char *)MALLOC(length);
	if (!buf)
	{
		fclose(in);
		return add_compile_error(options->back, options->inFilePath, 0, error_error, "Failed to allocate buffer");
	}
	fread_s(buf, length, sizeof(char), length, in);
	fclose(in);

	buffer_t *obuf = NEW_BUFFER(256);
	buffer_t *dbuf = options->debug ? NEW_BUFFER(256) : NULL;

	data_compile_options_t dataCompileOptions = {
		buf,
		length,
		obuf,
		options->inFilePath,
		options->back,
		options->version,
		options->debug,
		options->alignment,
		dbuf,
		options->classpaths
	};


	options->back = compile_data(&dataCompileOptions);

	int hasWarnings = 0;
	if (options->back)
	{
		compile_error_t *curr = options->back->front;
		while (curr)
		{
			if (curr->type == error_error)
			{
				options->back = add_compile_error(options->back, NULL, 0, error_info, "%s failed to build with errors.", options->inFilePath);
				return options->back->front;
			}
			else if (curr->type == error_warning)
				hasWarnings = 1;
			curr = curr->next;
		}
	}

	char destPath[MAX_PATH];
	strcpy_s(destPath, sizeof(destPath), options->outputDirectory);
	strcat_s(destPath, sizeof(destPath), "\\");
	strcat_s(destPath, sizeof(destPath), options->unitname);

	char *dirchr = strrchr(destPath, '\\');
	if (dirchr)
	{
		*dirchr = 0;
		create_intermediate_directories(destPath);
		*dirchr = '\\';
	}

	char *ext = strrchr(destPath, '.');
	if (ext)
	{
		ext[1] = 'l';
		ext[2] = 'b';
		ext[3] = 0;
	}
	else
	{
		strcat_s(destPath, sizeof(destPath), ".lb");
	}

	*options->outputFiles = add_file(*options->outputFiles, destPath, destPath);

	FILE *out = NULL;
	fopen_s(&out, destPath, "wb");
	if (!out)
	{
		FREE_BUFFER(obuf);
		return add_compile_error(options->back, options->inFilePath, 0, 0, "Failed to fopen for write", error_error);
	}
	fputc(0, out);
	fwrite(&options->version, sizeof(unsigned int), 1, out);
	fwrite(obuf->buf, sizeof(char), (size_t)(obuf->cursor - obuf->buf), out);

	fclose(out);
	FREE_BUFFER(obuf);

	if (options->debug)
	{
		if (!ext)
			ext = strrchr(destPath, '\\');
		
		ext[1] = 'l';
		ext[2] = 'd';
		ext[3] = 's';
		ext[4] = 0;

		FILE *dfout;
		fopen_s(&dfout, destPath, "wb");
		if (dfout)
		{
			fwrite(&options->version, sizeof(unsigned int), 1, dfout);
			fputs(options->unitname, dfout);
			fputc(0, dfout);
			fwrite(dbuf->buf, sizeof(char), (size_t)(dbuf->cursor - dbuf->buf), dfout);
			fclose(dfout);
		}
		else
			return add_compile_error(options->back, options->inFilePath, 0, error_warning, "Failed to fopen for write debug information");

		FREE_BUFFER(dbuf);
	}

	if (hasWarnings)
		options->back = add_compile_error(options->back, NULL, 0, error_info, "%s built with warnings.", options->inFilePath);
	else
		options->back = add_compile_error(options->back, NULL, 0, error_info, "%s successfully built.", options->inFilePath);

	FREE(buf);
	return options->back ? options->back->front : NULL;
}

compile_error_t *compile_data(data_compile_options_t *options)
{
	compile_state_t cs;
	line_t *formatted;
	size_t alignAmount;
	line_t *curr;
	size_t i;
	int packageAllowed, importsAllowed;

	cs.back = options->back;
	cs.out = options->out;
	cs.srcfile = options->srcFile;
	cs.package[0] = 0;

	formatted = format_document(options->data, options->datalen);

	cs.lscuctx = lscu_init();

	static const char LIB_DIR[] = "lib\\";
	char path[MAX_PATH];
	ZeroMemory(path, sizeof(path));

	if (!GetModuleFileNameA(NULL, path, sizeof(path)))
		return 0;

	char *libcursor = strrchr(path, '\\') + 1;
	strcpy_s(libcursor, sizeof(path) - (libcursor - path), LIB_DIR);
	lscu_add_unimportant_classpath(cs.lscuctx, path);

	for (i = 0; i < LSCU_MAX_CLASSPATHS; i++)
	{
		if (!options->classpaths[i][0]) break;
		lscu_add_classpath(cs.lscuctx, options->classpaths[i]);
	}
	
	packageAllowed = 1;
	importsAllowed = 1;

	curr = formatted;
	while (curr)
	{
		char *line = curr->line;
		if (*line != '#')
		{
			cs.srcline = curr->linenum;

			if (options->debug)
			{
				unsigned int off = (unsigned int)(options->out->cursor - options->out->buf);
				PUT_UINT(options->debugOut, off);
				PUT_INT(options->debugOut, cs.srcline);
			}

			cs.tokens = tokenize_string(line, &cs.tokencount);
			cs.cmd = get_command_byte(cs.tokens[0]);

			switch (cs.cmd)
			{
			case lb_package:
				if (!packageAllowed) cs.back = add_compile_error(cs.back, cs.srcfile, cs.srcline, error_error, "package not allowed here");
				else if (cs.package[0]) cs.back = add_compile_error(cs.back, cs.srcfile, cs.srcline, error_error, "Package redefinition");
				else if (cs.tokencount == 1) cs.back = add_compile_error(cs.back, cs.srcfile, cs.srcline, error_error, "Expected package name");
				else
				{
					strcpy_s(cs.package, sizeof(cs.package), cs.tokens[1]);
					lscu_set_package(cs.lscuctx, cs.package);
				}
				break;

			case lb_using:
				if (!importsAllowed) cs.back = add_compile_error(cs.back, cs.srcfile, cs.srcline, error_error, "using not allowed here");
				else if (cs.tokencount == 1) cs.back = add_compile_error(cs.back, cs.srcfile, cs.srcline, error_error, "Expected package name");
				else
				{
					lscu_add_import(cs.lscuctx, cs.tokens[1]);
					packageAllowed = 0;
				}
				break;

			case lb_class:
				packageAllowed = 0;
				importsAllowed = 0;
				lscu_add_import(cs.lscuctx, "lscript.lang");
				handle_class_def(&cs);
				break;

			case lb_global:
				alignAmount = options->alignment.globalAlignment - ((size_t)(cs.out->cursor - cs.out->buf) % (size_t)options->alignment.globalAlignment);
				cs.out = PUT_BYTES(cs.out, lb_align, alignAmount);
				handle_field_def(&cs);
				break;
			case lb_function:
				alignAmount = options->alignment.functionAlignment - ((size_t)(cs.out->cursor - cs.out->buf) % (size_t)options->alignment.functionAlignment);
				cs.out = PUT_BYTES(cs.out, lb_align, alignAmount);
				handle_function_def(&cs);
				break;
			case lb_constructor:
				alignAmount = options->alignment.functionAlignment - ((size_t)(cs.out->cursor - cs.out->buf) % (size_t)options->alignment.functionAlignment);
				cs.out = PUT_BYTES(cs.out, lb_align, alignAmount);
				handle_constructor_def(&cs);
				break;

			case lb_void:
				cs.back = add_compile_error(cs.back, cs.srcfile, cs.srcline, error_error, "void cannot be used as a storage specifier.");
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
				if (cs.tokencount == 2)
				{
					cs.out = PUT_BYTE(cs.out, cs.cmd);
					cs.out = PUT_STRING(cs.out, cs.tokens[1]);
				}
				else if (cs.tokencount == 1)
					cs.back = add_compile_error(cs.back, cs.srcfile, cs.srcline, error_error, "Missing variable name declaration");
				else
					cs.back = add_compile_error(cs.back, cs.srcfile, cs.srcline, error_warning, "Unecessary arguments following variable declaration");
				break;
			case lb_object:
			case lb_objectarray:
				if (cs.tokencount == 3)
				{
					if (lscu_resolve_class(cs.lscuctx, cs.tokens[1], NULL, 0))
					{
						cs.out = PUT_BYTE(cs.out, cs.cmd);
						cs.out = PUT_STRING(cs.out, cs.tokens[2]);
					}
					else
						cs.back = add_compile_error(cs.back, cs.srcfile, cs.srcline, error_error, "Unresolved symbol \"%s\"", cs.tokens[1]);
				}
				else if (cs.tokencount == 2)
				{
					if (!lscu_resolve_class(cs.lscuctx, cs.tokens[1], NULL, 0))
						cs.back = add_compile_error(cs.back, cs.srcfile, cs.srcline, error_error, "Unresolved symbol \"%s\"", cs.tokens[1]);
					cs.back = add_compile_error(cs.back, cs.srcfile, cs.srcline, error_error, "Missing variable name declaration");
				}
				else if (cs.tokencount == 1)
					cs.back = add_compile_error(cs.back, cs.srcfile, cs.srcline, error_error, "Missing variable object class");
				else
					cs.back = add_compile_error(cs.back, cs.srcfile, cs.srcline, error_warning, "Unecessary arguments following variable declaration");
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
				handle_set_cmd(&cs);
				break;

			case lb_castc:
			case lb_castuc:
			case lb_casts:
			case lb_castus:
			case lb_casti:
			case lb_castui:
			case lb_castl:
			case lb_castul:
			case lb_castb:
			case lb_castf:
			case lb_castd:
				handle_cast_cmd(&cs);
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
				handle_ret_cmd(&cs);
				break;

			case lb_static_call:
			case lb_dynamic_call:
				handle_call_cmd(&cs);
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
				handle_math_cmd(&cs);
				break;

			case lb_neg:
			case lb_not:
				handle_unary_math_cmd(&cs);
				break;

			case lb_if:
			case lb_while:
				handle_if_style_cmd(&cs);
				break;
			case lb_else:
				//out = put_byte(out, tokenCount > 1);
				if (cs.tokencount > 1)
				{
					cs.out = PUT_BYTE(cs.out, lb_elif);
					cs.out = PUT_LONG(cs.out, -1);

					if (strcmp(cs.tokens[1], "if"))
					{
						cs.back = add_compile_error(cs.back, cs.srcfile, cs.srcline, error_error, "Unexpected token \"%s\"", cs.tokens[1]);
						break;
					}
					cs.tokens += 1;
					cs.tokencount -= 1;
					cs.cmd = lb_if;

					handle_if_style_cmd(&cs);

					cs.cmd = lb_else;
					cs.tokencount += 1;
					cs.tokens -= 1;
				}
				else
				{
					options->out = PUT_BYTE(options->out, lb_else);
					options->out = PUT_LONG(options->out, -1);
				}
				break;
			case lb_end:
				cs.out = PUT_BYTE(cs.out, cs.cmd);
				cs.out = PUT_LONG(cs.out, -1);
				break;
			default:
				cs.back = add_compile_error(cs.back, cs.srcfile, cs.srcline, error_error, "Unknown command \"%s\"", cs.tokens[0]);
				break;
			}

			free_tokenized_data(cs.tokens, cs.tokencount);
		}

		curr = curr->next;
	}

	free_formatted(formatted);

	lscu_destroy(cs.lscuctx);
	return cs.back;
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
	else if (!strcmp(string, "package"))
		return lb_package;
	else if (!strcmp(string, "using"))
		return lb_using;
	else if (!strcmp(string, "constructor"))
		return lb_constructor;

	else if (!strcmp(string, "void"))
		return lb_void;
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

	else if (!strcmp(string, "castc"))
		return lb_castc;
	else if (!strcmp(string, "castuc"))
		return lb_castuc;
	else if (!strcmp(string, "casts"))
		return lb_casts;
	else if (!strcmp(string, "castus"))
		return lb_castus;
	else if (!strcmp(string, "casti"))
		return lb_casti;
	else if (!strcmp(string, "castui"))
		return lb_castui;
	else if (!strcmp(string, "castl"))
		return lb_castl;
	else if (!strcmp(string, "castul"))
		return lb_castul;
	else if (!strcmp(string, "castb"))
		return lb_castb;
	else if (!strcmp(string, "castf"))
		return lb_castf;
	else if (!strcmp(string, "castd"))
		return lb_castd;

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
		case '(':
		case ')':
		case ',':
			if (inDoubleQuotes || inSingleQuotes)
				PUT_CHAR(currstring, c);
			else
			{
				char *curbuf;
				if (currstring->cursor > currstring->buf)
				{
					currstring = PUT_CHAR(currstring, 0);
					curbuf = (char *)MALLOC((size_t)(currstring->cursor - currstring->buf));
					if (!curbuf)
						return NULL;
					MEMCPY(curbuf, currstring->buf, (size_t)(currstring->cursor - currstring->buf));

					list = PUT_ULONG(list, (size_t)curbuf);
					(*tokenCount)++;
				}

				// Put the delimeter as a new token

				curbuf = (char *)MALLOC(2);
				if (!curbuf)
					return NULL;
				curbuf[0] = c;
				curbuf[1] = 0;

				list = PUT_ULONG(list, (size_t)curbuf);
				(*tokenCount)++;

				FREE_BUFFER(currstring);
				currstring = NEW_BUFFER(32);
			}
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
		*isAbsoluteType = 2;
		if (!strcmp(mString, "true"))
		{
			*type = lb_byte;
			data->ulvalue = 1;
			return sizeof(byte_t);
		}
		else if (!strcmp(mString, "false"))
		{
			*type = lb_byte;
			data->ulavalue = 0;
			return sizeof(byte_t);
		}
		else if (!strcmp(mString, "null"))
		{
			*type = lb_qword;
			data->ovalue = NULL;
			return sizeof(qword_t);
		}

		*isAbsoluteType = 0;
		return 0;
	}
	char *dataStart = lBracket + 1;
	char *rBracket = strrchr(dataStart, ']');
	if (!rBracket)
		return 0;
	*lBracket = 0;
	*rBracket = 0;
	int size;

	*isAbsoluteType = 0;
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
		data->ivalue = (lint)atoll(dataStart);
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
	else if (!strcmp(stringType, "void"))
		return lb_void;
	return 0;
}

byte_t resolve_data_type(compile_state_t *state, char *declared, char *classname, size_t namelen)
{
	byte_t returnType = get_command_byte(declared);
	if (returnType < lb_void || returnType > lb_objectarray)
	{
		returnType = lb_object;

		// Test for array
		char *startBracket = strchr(declared, '[');
		if (startBracket)
		{
			char *shouldEnd = startBracket + 1;
			if (!*shouldEnd || *shouldEnd != ']')
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected \"]\"");
				return 0;
			}

			if (*(shouldEnd + 1))
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Malformed type \"%s\"");
				return 0;
			}

			*startBracket = 0;

			returnType = lb_objectarray;
		}

		if (!lscu_resolve_class(state->lscuctx, declared, classname, namelen))
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unresolved symbol \"%s\"", state->tokens[3]);
			return 0;
		}

		if (startBracket) *startBracket = '[';
	}

	return returnType;
}

void handle_class_def(compile_state_t *state)
{
	if (state->tokencount < 2)
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected token class name declaration");
		return;
	}
	
	const char *classname = state->tokens[1];
	PUT_BYTE(state->out, lb_class);
	if (state->package[0])
	{
		char *cursor = state->package;
		while (*cursor)
		{
			PUT_BYTE(state->out, *cursor);
			cursor++;
		}
		PUT_BYTE(state->out, '.');
	}
	PUT_STRING(state->out, classname);
	if (state->tokencount > 2)
	{
		if (strcmp(state->tokens[2], "extends"))
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexpected token \"%s\", \"extends\" expected.", state->tokens[2]);
			return;
		}

		if (state->tokencount == 3)
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected token superclass name declaration");
			return;
		}
		else if (state->tokencount == 4)
		{
			if (strcmp(state->tokens[3], "null"))
			{
				PUT_BYTE(state->out, lb_extends);
				char *superclassname = state->tokens[3];

				char fullname[MAX_PATH];
				if (!lscu_resolve_class(state->lscuctx, superclassname, fullname, sizeof(fullname)))
				{
					state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unresolved symbol \"%s\"", superclassname);
					return;
				}

				PUT_STRING(state->out, fullname);
			}
		}
		else
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_warning, "Uneceassary arguments following class declaration");
			return;
		}
	}
	else
	{
		PUT_BYTE(state->out, lb_extends);
		PUT_STRING(state->out, "lscript.lang.Object");
	}
}

void handle_field_def(compile_state_t *state)
{
	if (state->tokencount < 5)
	{
		switch (state->tokencount)
		{
		case 1:
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected global storage specifier");
			break;
		case 2:
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected global write permissions specifier");
			break;
		case 3:
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected global data type specifier");
			break;
		case 4:
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected global name declaration");
			break;
		}
	}
	int isStatic, isVarying;
	byte_t dataType;
	const char *name;

	if (!strcmp(state->tokens[1], "static"))
		isStatic = 1;
	else if (!strcmp(state->tokens[1], "dynamic"))
		isStatic = 0;
	else
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexpected token \"%s\", expected \"static\" or \"dynamic\"", state->tokens[1]);
		return;
	}

	if (!strcmp(state->tokens[2], "varying"))
		isVarying = 1;
	else if (!strcmp(state->tokens[2], "const"))
		isVarying = 0;
	else
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexpected token \"%s\", expected \"varying\" or \"const\"", state->tokens[1]);
		return;
	}

	dataType = get_command_byte(state->tokens[3]);
	if (dataType < lb_char || dataType > lb_objectarray)
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Invalid global data type specifier \"%s\"", state->tokens[3]);
		return;
	}

	name = state->tokens[4];

	if (isStatic && state->tokencount < 6)
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Global declared static must have an initializer");
		return;
	}
	else if (!isStatic && state->tokencount > 5)
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Global declared dynamic cannot have an initializer");
		return;
	}

	if (state->tokencount > 5)
	{
		data_t data;
		byte_t type;
		int isAbsolute;
		int initSize = (int)evaluate_constant(state->tokens[5], &data, &type, &isAbsolute);

		if (!initSize)
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Initialization of globals to variables is not supported");
			return;
		}

		byte_t neededType;
		size_t neededSize = get_type_properties(dataType, &neededType);

		if (neededSize != initSize)
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Type size mismatch");
			return;
		}

		if (neededType != type)
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_warning, "Value types do not match");

		PUT_BYTE(state->out, lb_global);
		PUT_STRING(state->out, name);
		PUT_BYTE(state->out, isStatic ? lb_static : lb_dynamic);
		PUT_BYTE(state->out, isVarying ? lb_varying : lb_const);
		PUT_BYTE(state->out, 0); PUT_BYTE(state->out, 0); PUT_BYTE(state->out, 0); PUT_BYTE(state->out, 0); PUT_BYTE(state->out, 0);
		PUT_BYTE(state->out, dataType);
		switch (neededType)
		{
		case lb_byte:
			PUT_BYTE(state->out, data.cvalue);
			break;
		case lb_word:
			PUT_SHORT(state->out, data.svalue);
			break;
		case lb_dword:
			PUT_INT(state->out, data.ivalue);
			break;
		case lb_qword:
			PUT_LONG(state->out, data.lvalue);
			break;
		case lb_real4:
			PUT_FLOAT(state->out, data.fvalue);
			break;
		case lb_real8:
			PUT_DOUBLE(state->out, data.dvalue);
			break;
		default:
			// something went wrong!
			assert(0);
			break;
		}

		if (state->tokencount > 6)
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_warning, "Unecessary arguments following global declaration");
	}
	else
	{
		PUT_BYTE(state->out, lb_global);
		PUT_STRING(state->out, name);
		PUT_BYTE(state->out, isStatic ? lb_static : lb_dynamic);
		PUT_BYTE(state->out, isVarying ? lb_varying : lb_const);
		PUT_BYTE(state->out, 0); PUT_BYTE(state->out, 0); PUT_BYTE(state->out, 0); PUT_BYTE(state->out, 0); PUT_BYTE(state->out, 0);
		PUT_BYTE(state->out, dataType);
	}
}

void handle_function_def(compile_state_t *state)
{
	int isStatic;
	byte_t execType = 0;
	byte_t returnType = 0;

	if (state->tokencount < 2)
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected return type");
		return;
	}

	if (!strcmp(state->tokens[1], "static"))
		isStatic = 1;
	else if (!strcmp(state->tokens[1], "dynamic"))
		isStatic = 0;
	else if (!strcmp(state->tokens[1], "abstract"))
	{
		isStatic = 0;
		execType = lb_abstract;
	}
	else if (!strcmp(state->tokens[1], "interp") || !strcmp(state->tokens[1], "native"))
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "%s not allowed here", state->tokens[3]);
		return;
	}
	else if (!(returnType = resolve_data_type(state, state->tokens[1], NULL, 0))) return;
	else
	{
		isStatic = 0;
		execType = lb_interp;
	}

	char *funcName;
	int argStart;

	if (returnType)
	{
		if (state->tokencount < 3)
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected function name declaration");
			return;
		}
		funcName = state->tokens[2];

		if (state->tokencount < 4)
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected \"(\"");
			return;
		}
		
		if (strcmp(state->tokens[3], "("))
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexpected token \"%s\"", state->tokens[3]);
			return;
		}

		argStart = 4;
	}
	else if (execType)
	{
		assert(execType == lb_abstract);

		if (state->tokencount < 3)
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected return type");
			return;
		}

		if (!(returnType = resolve_data_type(state, state->tokens[2], NULL, 0))) return;

		if (state->tokencount < 4)
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected function name declaration");
			return;
		}

		funcName = state->tokens[3];

		if (state->tokencount < 5)
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected \"(\"");
			return;
		}

		if (strcmp(state->tokens[4], "("))
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexpected token \"%s\"", state->tokens[4]);
			return;
		}

		argStart = 5;
	}
	else
	{
		if (state->tokencount < 3)
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected \"interp\", \"native\", or \"abstract\"");
			return;
		}
		if (!strcmp(state->tokens[2], "interp")) execType = lb_interp;
		else if (!strcmp(state->tokens[2], "native")) execType = lb_native;
		else if (!strcmp(state->tokens[2], "abstract")) execType = lb_abstract;
		else if (returnType = resolve_data_type(state, state->tokens[2], NULL, 0))
			execType = lb_interp;
		else return;

		if (!returnType)
		{
			if (execType == lb_abstract && isStatic)
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "static function cannot be declared as abstract");
				return;
			}

			if (state->tokencount < 4)
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected return type");
				return;
			}

			if (!(returnType = resolve_data_type(state, state->tokens[3], NULL, 0))) return;

			if (state->tokencount < 5)
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected function name declaration");
				return;
			}

			funcName = state->tokens[4];

			if (state->tokencount < 6)
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected \"(\"");
				return;
			}

			if (strcmp(state->tokens[5], "("))
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexpected token \"%s\"", state->tokens[4]);
				return;
			}

			argStart = 6;
		}
		else
		{
			if (state->tokencount < 4)
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected function name declaration");
				return;
			}

			funcName = state->tokens[3];

			if (state->tokencount < 5)
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected \"(\"");
				return;
			}

			if (strcmp(state->tokens[4], "("))
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexpected token \"%s\"", state->tokens[4]);
				return;
			}

			argStart = 5;
		}
	}

	if (state->tokencount <= argStart)
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected \")\"");
		return;
	}

	buffer_t *build = NEW_BUFFER(64);

	byte_t argType;
	char classname[MAX_PATH] = { 0 };
	int writeClassname;
	char *argumentName;

	byte_t argCount = 0;
	for (size_t i = argStart;; i++, argCount++)
	{
		if (!strcmp(state->tokens[i], ")")) break;
		if (i >= state->tokencount)
		{
			FREE_BUFFER(build);
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexpected end of function declaration");
			return;
		}

		writeClassname = 1;
		argType = get_command_byte(state->tokens[i]);
		if (argType < lb_char || argType > lb_objectarray)
		{
			if (!(argType = resolve_data_type(state, state->tokens[i], classname, sizeof(classname)))) return;
		}
		else if (argType == lb_object || argType == lb_objectarray)
		{
			i++;
			if (i == state->tokencount)
			{
				FREE_BUFFER(build);
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected function argument classname");
				return;
			}

			if (!lscu_resolve_class(state->lscuctx, state->tokens[i], classname, sizeof(classname)))
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unresolved symbol \"%s\"", state->tokens[i]);
				return;
			}
		}
		else
			writeClassname = 0;

		i++;
		if (i == state->tokencount)
		{
			FREE_BUFFER(build);
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected function argument name");
			return;
		}

		argumentName = state->tokens[i];

		PUT_BYTE(build, argType);
		if (writeClassname) PUT_STRING(build, classname);
		PUT_STRING(build, argumentName);

		// Require ',' separating arguments (exclude last argument)
		if (i < state->tokencount - 2)
		{
			i++;
			if (strcmp(state->tokens[i], ","))
			{
				FREE_BUFFER(build);
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexepcted token \"%s\"", state->tokens[i]);
				return;
			}
		}
	}

	PUT_BYTE(state->out, lb_function);
	PUT_BYTE(state->out, isStatic ? lb_static : lb_dynamic);
	PUT_BYTE(state->out, execType);
	PUT_BYTE(state->out, returnType);
	PUT_STRING(state->out, funcName);
	PUT_BYTE(state->out, argCount);
	PUT_BUF(state->out, build);

	FREE_BUFFER(build);
}

void handle_constructor_def(compile_state_t *state)
{
	if (state->tokencount < 3)
	{
		switch (state->tokencount)
		{
		case 1:
			state->back = add_compile_error(state->back, state->srcline, state->srcline, error_error, "Expected \"(\"");
			break;
		case 2:
			state->back = add_compile_error(state->back, state->srcline, state->srcline, error_error, "Expected formal arguments");
			break;
		}
		return;
	}

	buffer_t *build = NEW_BUFFER(64);

	if (strcmp(state->tokens[1], "("))
	{
		state->back = add_compile_error(state->back, state->srcline, state->srcline, error_error, "Unexpected token \"%s\"", state->tokens[5]);
		return;
	}

	byte_t argCount = 0;
	for (size_t i = 2;; i++, argCount++)
	{
		if (!strcmp(state->tokens[i], ")")) break;
		if (i >= state->tokencount)
		{
			FREE_BUFFER(build);
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexpected end of function declaration");
			return;
		}

		byte_t type = get_command_byte(state->tokens[i]);
		if (type < lb_char || type > lb_objectarray)
		{
			FREE_BUFFER(build);
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Illegal argument type");
			return;
		}

		const char *argName;

		i++;
		if (type == lb_object || type == lb_objectarray)
		{
			if (i == state->tokencount)
			{
				FREE_BUFFER(build);
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected function argument object classname");
				return;
			}

			const char *classname = state->tokens[i];

			char fullname[MAX_PATH];
			if (!lscu_resolve_class(state->lscuctx, classname, fullname, sizeof(fullname)))
			{
				FREE_BUFFER(build);
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unresolved symbol \"%s\"", state->tokens[i]);
				return;
			}

			i++;
			if (i == state->tokencount)
			{
				FREE_BUFFER(build);
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected function argument name");
				return;
			}

			argName = state->tokens[i];

			PUT_BYTE(build, type);
			PUT_STRING(build, fullname);
			PUT_STRING(build, argName);
		}
		else if (i == state->tokencount)
		{
			FREE_BUFFER(build);
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected function argument name");
			return;
		}
		else
		{
			argName = state->tokens[i];

			PUT_BYTE(build, type);
			PUT_STRING(build, argName);
		}

		// Require ',' separating arguments (exclude last argument)
		if (i < state->tokencount - 2)
		{
			i++;
			if (strcmp(state->tokens[i], ","))
			{
				FREE_BUFFER(build);
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexepcted token \"%s\"", state->tokens[i]);
				return;
			}
		}
	}

	static const char CONSTRUCTOR_NAME[] = "<init>";

	PUT_BYTE(state->out, lb_function);
	PUT_BYTE(state->out, lb_dynamic);
	PUT_BYTE(state->out, lb_interp);
	PUT_BYTE(state->out, lb_void);
	PUT_STRING(state->out, CONSTRUCTOR_NAME);
	PUT_BYTE(state->out, argCount);
	PUT_BUF(state->out, build);

	FREE_BUFFER(build);
}

void handle_set_cmd(compile_state_t *state)
{
	if (state->tokencount < 2)
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected variable name");
		return;
	}

	const char *varname = state->tokens[1];
	int myWidth;
	byte_t myType;
	data_t setData;
	byte_t setType;
	size_t setWidth;
	int setIsAbsolute;

	if (state->cmd != lb_setr && state->tokencount < 3)
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expeceted value");
		return;
	}

	switch (state->cmd)
	{
	case lb_setv:
		PUT_BYTE(state->out, lb_setv);
		PUT_STRING(state->out, varname);
		//put_byte(out, lb_value);
		PUT_STRING(state->out, state->tokens[2]);
		break;
	case lb_seto:
		if (!strcmp(state->tokens[2], "new"))
		{
			if (state->tokencount < 6)
			{
				switch (state->tokencount)
				{
				case 4:
					state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected instantiated class name");
					break;
				case 5:
					state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected \")\"");
					break;
				}
				return;
			}
			
			if (strcmp(state->tokens[4], "("))
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexpected token \"%s\"", state->tokens[5]);
				return;
			}

			char *classname = state->tokens[3];
			char fullname[MAX_PATH];
			if (!lscu_resolve_class(state->lscuctx, classname, fullname, sizeof(fullname)))
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unresolved symbol \"%s\"", classname);
				return;
			}
			
			static const char CONSTRUCTOR_FUNCNAME[] = "<init>(";
			if (!strcmp(state->tokens[5], ")"))
			{
				PUT_BYTE(state->out, lb_seto);
				PUT_STRING(state->out, varname);
				PUT_BYTE(state->out, lb_new);
				PUT_STRING(state->out, fullname);

				char towrite[MAX_PATH];
				strcpy_s(towrite, sizeof(towrite), CONSTRUCTOR_FUNCNAME);

				PUT_STRING(state->out, towrite);
				return;
			}

			if (state->tokencount < 8)
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected \")\"");
				return;
			}

			char temp[MAX_PATH];
			char qualifiedfuncname[MAX_PATH];
			char *qfnCursor;

			strcpy_s(qualifiedfuncname, sizeof(qualifiedfuncname), CONSTRUCTOR_FUNCNAME);
			qfnCursor = qualifiedfuncname + strlen(qualifiedfuncname);

			char *sig = state->tokens[5];

			size_t argcount = 0;
			while (*sig)
			{
				if (*sig == '[')
				{
					*qfnCursor = *sig;
					qfnCursor++;
					sig++;
				}

				if (!*sig)
				{
					state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexpected end of function signature");
					return;
				}

				if (*sig == 'L')
				{
					*qfnCursor = *sig;
					qfnCursor++;
					sig++;

					char *saved = sig;

					while (*sig && *sig != ';') sig++;

					if (!*sig)
					{
						state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexpected end of function signature");
						return;
					}

					*sig = 0;

					if (!lscu_resolve_class(state->lscuctx, saved, temp, sizeof(temp)))
					{
						state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unresolved symbol \"%s\"", saved);
						return;
					}

					*sig = ';';

					char *tempcursor = temp;
					while (*tempcursor)
					{
						*qfnCursor = *tempcursor;
						tempcursor++;
						qfnCursor++;
					}
					*qfnCursor = ';';

					qfnCursor++;
					sig++;
				}
				else
				{
					*qfnCursor = *sig;
					qfnCursor++;
					sig++;
				}
				argcount++;
			}

			if (strcmp(state->tokens[6], ")"))
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexpected token \"%s\"", state->tokens[4]);
				return;
			}

			*qfnCursor = 0;

			buffer_t *argBuffer = NEW_BUFFER(128);

			size_t i, currarrg;
			for (currarrg = 0, i = 7; i < state->tokencount; currarrg++, i++)
			{
				if (currarrg == argcount)
				{
					state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Too many arguments for constructor %s", qualifiedfuncname);
					FREE_BUFFER(argBuffer);
					return;
				}

				data_t argData;
				byte_t argType;
				int isAbsoluteType;
				size_t argSize = evaluate_constant(state->tokens[i], &argData, &argType, &isAbsoluteType);

				if (argSize == 0)
				{
					if (state->tokens[i][0] == SIG_STRING_CHAR)
					{
						PUT_BYTE(argBuffer, lb_string);
						PUT_STRING(argBuffer, state->tokens[i] + 1);
					}
					else if (state->tokens[i][0] == SIG_CHAR_CHAR)
					{
						PUT_BYTE(argBuffer, lb_byte);
						PUT_BYTE(argBuffer, state->tokens[i][1]);
					}
					else
					{
						if (!strcmp(state->tokens[i], "ret"))
						{
							PUT_BYTE(argBuffer, lb_ret);
						}
						else
						{
							PUT_BYTE(argBuffer, lb_value);
							PUT_STRING(argBuffer, state->tokens[i]);
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

			if (currarrg != argcount)
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Not enough arguments for constructor %s", qualifiedfuncname);
				FREE_BUFFER(argBuffer);
				return;
			}

			PUT_BYTE(state->out, lb_seto);
			PUT_STRING(state->out, varname);
			PUT_BYTE(state->out, lb_new);
			PUT_STRING(state->out, fullname);
			PUT_STRING(state->out, qualifiedfuncname);
			PUT_BUF(state->out, argBuffer);

			FREE_BUFFER(argBuffer);
		}
		else if (!strcmp(state->tokens[2], "null"))
		{
			PUT_BYTE(state->out, lb_seto);
			PUT_STRING(state->out, varname);
			PUT_BYTE(state->out, lb_null);
		}
		else if (get_primitive_type(state->tokens[2]))
		{
			PUT_BYTE(state->out, lb_seto);
			PUT_STRING(state->out, varname);
			handle_array_creation(state);
		}
		else if (state->tokens[2][0] == SIG_STRING_CHAR)
		{
			PUT_BYTE(state->out, lb_seto);
			PUT_STRING(state->out, varname);
			PUT_BYTE(state->out, lb_string);
			PUT_STRING(state->out, state->tokens[2] + 1);
		}
		else
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Invalid usage of seto; must be initialization of object, array, string, or null");
			return;
		}

		break;
	case lb_setr:
		PUT_BYTE(state->out, lb_setr);
		PUT_STRING(state->out, varname);
		break;
	default:
		myType = state->cmd + (lb_byte - lb_setb);
		myWidth = (int)get_type_width(myType);

		if (state->tokens[2][0] == SIG_CHAR_CHAR)
		{
			setData.cvalue = state->tokens[2][1];
			setWidth = sizeof(lchar);
			setType = lb_byte;
		}
		else
		{
			setWidth = evaluate_constant(state->tokens[2], &setData, &setType, &setIsAbsolute);
		}

		if (myWidth != setWidth)
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Type size mismatch (got: %d, needs: %d)", (int)setWidth, (int)myWidth);
			return;
		}

		if (myType != setType)
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_warning, "Value types do not match");

		PUT_BYTE(state->out, state->cmd);
		PUT_STRING(state->out, varname);
		//put_byte(out, myType);

		switch (myType)
		{
		case lb_byte:
			PUT_CHAR(state->out, setData.cvalue);
			break;
		case lb_word:
			PUT_SHORT(state->out, setData.svalue);
			break;
		case lb_dword:
			PUT_INT(state->out, setData.ivalue);
			break;
		case lb_qword:
			PUT_LONG(state->out, setData.lvalue);
			break;
		case lb_real4:
			PUT_FLOAT(state->out, setData.fvalue);
			break;
		case lb_real8:
			PUT_DOUBLE(state->out, setData.dvalue);
			break;
		}

		break;
	}
}

void handle_cast_cmd(compile_state_t *state)
{
	if (state->tokencount < 2)
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected destination variable name");
		return;
	}

	if (state->tokencount < 3)
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected source variable name");
		return;
	}

	PUT_CHAR(state->out, state->cmd);
	PUT_STRING(state->out, state->tokens[1]);
	PUT_STRING(state->out, state->tokens[2]);

	return state->back;
}

void handle_array_creation(compile_state_t *state)
{
	if (state->tokencount == 2)
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Not enough array initialization arguments");
		return;
	}
	else if (state->tokencount == 3)
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected array length specifier");
		return;
	}

	byte_t type = get_primitive_type(state->tokens[2]);

	data_t argData;
	byte_t argType;
	int argIsAbsolute;
	size_t argSize = evaluate_constant(state->tokens[3], &argData, &argType, &argIsAbsolute);
	if (argIsAbsolute)
		return add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Array initialization does not support absolute type");
	////else if (argSize == 0)
		//return add_compile_error(back, srcFile, srcLine, error_error, "Invalid array length specifier");
	PUT_BYTE(state->out, type);
	if (argSize == 0)
	{
		PUT_BYTE(state->out, lb_value);
		PUT_STRING(state->out, state->tokens[3]);
	}
	else
	{
		switch (argType)
		{
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
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_warning, "Arrays have maximum 32-bit unsigned length but requested length is 64-bit");
			break;
		case lb_real4:
		case lb_real8:
			return add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Array initialization requires an integral length type");
			break;
		}

		PUT_BYTE(state->out, lb_dword);
		PUT_INT(state->out, argData.uivalue);
	}

	if (state->tokencount > 4)
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_warning, "Unecessary arguments in array initialization");
}

void handle_ret_cmd(compile_state_t *state)
{
	size_t valueRetDesSize;
	byte_t valueRetDesType;
	switch (state->cmd)
	{
	case lb_ret:
	case lb_retr:
		PUT_BYTE(state->out, state->cmd);
		if (state->tokencount > 1)
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_warning, "Unecessary arguments following function return");
		break;
	case lb_retv:
		if (state->tokencount < 2)
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected variable name");
			return;
		}

		PUT_BYTE(state->out, lb_retv);
		PUT_STRING(state->out, state->tokens[1]);

		if (state->tokencount > 2)
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_warning, "Unecessary arguments following function return");
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
		if (state->tokencount < 2)
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected constant");
			return;
		}
		else
		{
			data_t retData;
			byte_t retType;
			int retIsAbsoluteType;
			size_t retSize = evaluate_constant(state->tokens[1], &retData, &retType, &retIsAbsoluteType);
			if (retIsAbsoluteType == 1)
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Absolute type specifier not supported on return statement");
				return;
			}

			if (retSize != valueRetDesSize)
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Type size mismatch (got: %d, needs: %d)", (int)retSize, (int)valueRetDesSize);
				return;
			}

			if (retType != valueRetDesType)
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_warning, "Value types do not match");

			PUT_BYTE(state->out, state->cmd);
			switch (retType)
			{
			case lb_byte:
				PUT_CHAR(state->out, retData.cvalue);
				break;
			case lb_word:
				PUT_SHORT(state->out, retData.svalue);
				break;
			case lb_dword:
				PUT_INT(state->out, retData.ivalue);
				break;
			case lb_qword:
				PUT_LONG(state->out, retData.lvalue);
				break;
			case lb_real4:
				PUT_FLOAT(state->out, retData.fvalue);
				break;
			case lb_real8:
				PUT_DOUBLE(state->out, retData.dvalue);
				break;
			}
		}

		if (state->tokencount > 2)
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_warning, "Unecessary arguments following function return");

		break;
	}
}

void handle_call_cmd(compile_state_t *state)
{
	if (state->tokencount < 4)
	{
		switch (state->tokencount)
		{
		case 1:
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected function name");
			break;
		case 2:
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected \"(\"");
			break;
		case 3:
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected function signature");
			break;
		}
		return;
	}

	if (strcmp(state->tokens[2], "("))
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexpected token \"%s\"", state->tokens[2]);
		return;
	}

	char *functionName = state->tokens[1];
	char unqualbuf[MAX_PATH];
	char temp[MAX_PATH];

	// If the first part of the function call is a class, replace with qualified name
	char *dot = strchr(functionName, '.');
	if (dot)
	{
		*dot = 0;
		if (lscu_resolve_class(state->lscuctx, functionName, temp, sizeof(temp)))
		{
			// Replace class name with qualified name
			*dot = '.';
			strcpy_s(unqualbuf, sizeof(unqualbuf), temp);
			strcat_s(unqualbuf, sizeof(unqualbuf), dot);
		}
		else
		{
			// Otherwise just leave the call as-is
			*dot = '.';
			strcpy_s(unqualbuf, sizeof(unqualbuf), functionName);
		}
	}
	else strcpy_s(unqualbuf, sizeof(unqualbuf), functionName);

	functionName = unqualbuf;

	if (!strcmp(functionName, "<init>"))
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_warning, "Calling constructor as function");

	if (!strcmp(state->tokens[3], ")"))
	{
		PUT_BYTE(state->out, state->cmd);

		char towrite[MAX_PATH];
		strcpy_s(towrite, sizeof(towrite), functionName);
		strcat_s(towrite, sizeof(towrite), "(");

		PUT_STRING(state->out, towrite);
		return;
	}

	char qualifiedfuncname[MAX_PATH];
	char *qfnCursor;

	size_t namelen = strlen(functionName);
	memcpy(qualifiedfuncname, functionName, namelen);
	qfnCursor = qualifiedfuncname + namelen;
	*qfnCursor = '('; qfnCursor++;

	char *sig = state->tokens[3];

	size_t argcount = 0;
	while (*sig)
	{
		if (*sig == '[')
		{
			*qfnCursor = *sig;
			qfnCursor++;
			sig++;
		}

		if (!*sig)
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexpected end of function signature");
			return;
		}

		if (*sig == 'L')
		{
			*qfnCursor = *sig;
			qfnCursor++;
			sig++;

			char *saved = sig;

			while (*sig && *sig != ';') sig++;

			if (!*sig)
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexpected end of function signature");
				return;
			}

			*sig = 0;

			if (!lscu_resolve_class(state->lscuctx, saved, temp, sizeof(temp)))
			{
				state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unresolved symbol \"%s\"", saved);
				return;
			}

			*sig = ';';

			char *tempcursor = temp;
			while (*tempcursor)
			{
				*qfnCursor = *tempcursor;
				tempcursor++;
				qfnCursor++;
			}
			*qfnCursor = ';';

			qfnCursor++;
			sig++;
		}
		else
		{
			*qfnCursor = *sig;
			qfnCursor++;
			sig++;
		}
		argcount++;
	}

	if (strcmp(state->tokens[4], ")"))
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexpected token \"%s\"", state->tokens[4]);
		return;
	}

	*qfnCursor = 0;

	buffer_t *argBuffer = NEW_BUFFER(128);
	
	size_t i, currarrg;
	for (currarrg = 0, i = 5; i < state->tokencount; currarrg++, i++)
	{
		if (currarrg == argcount)
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Too many arguments for function %s", qualifiedfuncname);
			FREE_BUFFER(argBuffer);
			return;
		}

		data_t argData;
		byte_t argType;
		int isAbsoluteType;
		size_t argSize = evaluate_constant(state->tokens[i], &argData, &argType, &isAbsoluteType);

		if (argSize == 0)
		{
			if (state->tokens[i][0] == SIG_STRING_CHAR)
			{
				PUT_BYTE(argBuffer, lb_string);
				PUT_STRING(argBuffer, state->tokens[i] + 1);
			}
			else if (state->tokens[i][0] == SIG_CHAR_CHAR)
			{
				PUT_BYTE(argBuffer, lb_byte);
				PUT_BYTE(argBuffer, state->tokens[i][1]);
			}
			else
			{
				if (!strcmp(state->tokens[i], "ret"))
				{
					PUT_BYTE(argBuffer, lb_ret);
				}
				else
				{
					PUT_BYTE(argBuffer, lb_value);
					PUT_STRING(argBuffer, state->tokens[i]);
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

	if (currarrg != argcount)
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Not enough arguments for function %s", qualifiedfuncname);
		FREE_BUFFER(argBuffer);
		return;
	}

	PUT_BYTE(state->out, state->cmd);
	PUT_STRING(state->out, qualifiedfuncname);
	PUT_BUF(state->out, argBuffer);

	//qualifiedfuncname[length - 1] = ')';

	FREE_BUFFER(argBuffer);
}

void handle_math_cmd(compile_state_t *state)
{
	if (state->tokencount < 4)
	{
		switch (state->tokencount)
		{
		case 1:
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected destination variable name");
			break;
		case 2:
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected source variable name");
			break;
		case 3:
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected operation argument");
			break;
		}
		return;
	}

	const char *dstVar = state->tokens[1];
	const char *srcVar = state->tokens[2];

	data_t argData;
	byte_t argType;
	int argIsAbsolute;
	size_t argSize;
	
	if (state->tokens[3][0] == SIG_STRING_CHAR)
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Operation requires integral or floating point type");
		return;
	}
	else if (state->tokens[3][0] == SIG_CHAR_CHAR)
	{
		argData.cvalue = state->tokens[3][1];
		argType = lb_char;
		argIsAbsolute = 1;
		argSize = sizeof(lchar);
	}
	else
	{
		argSize = evaluate_constant(state->tokens[3], &argData, &argType, &argIsAbsolute);

		if (argSize != 0 && !argIsAbsolute)
		{
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Operation requires absolute type");
			return;
		}
	}

	PUT_BYTE(state->out, state->cmd);
	PUT_STRING(state->out, dstVar);
	PUT_STRING(state->out, srcVar);

	if (argSize == 0)
	{
		const char *srcVar2 = state->tokens[3];
		PUT_BYTE(state->out, lb_value);
		PUT_STRING(state->out, srcVar2);
	}
	else
	{
		PUT_BYTE(state->out, argType);

		byte_t type;
		get_type_properties(argType, &type);
		switch (type)
		{
		case lb_byte:
			PUT_CHAR(state->out, argData.cvalue);
			break;
		case lb_word:
			PUT_SHORT(state->out, argData.svalue);
			break;
		case lb_dword:
			PUT_INT(state->out, argData.ivalue);
			break;
		case lb_qword:
			PUT_LONG(state->out, argData.lvalue);
			break;
		case lb_real4:
			PUT_FLOAT(state->out, argData.fvalue);
			break;
		case lb_real8:
			PUT_DOUBLE(state->out, argData.dvalue);
			break;
		}
	}

	if (state->tokencount > 4)
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_warning, "Unecessary arguments following operation");
}

void handle_unary_math_cmd(compile_state_t *state)
{
	if (state->tokencount < 3)
	{
		switch (state->tokencount)
		{
		case 1:
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected destination variable name");
			break;
		case 2:
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected source variable name");
			break;
		}
		return;
	}

	const char *dstVar = state->tokens[1];
	const char *srcVar = state->tokens[2];

	PUT_BYTE(state->out, state->cmd);
	PUT_STRING(state->out, dstVar);
	PUT_STRING(state->out, srcVar);

	if (state->tokencount > 3)
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_warning, "Unecessary arguments following unary operation");
		return;
	}
}

void handle_if_style_cmd(compile_state_t *state)
{
	if (state->tokencount < 2)
	{
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected variable name");
		return;
	}

	buffer_t *temp = NEW_BUFFER(32);
	byte_t count = lb_one;

	data_t lhsData;
	byte_t lhsType;
	int lhsIsAbsolute;
	size_t lhsSize = evaluate_constant(state->tokens[1], &lhsData, &lhsType, &lhsIsAbsolute);

	if (lhsSize == 0 && !lhsIsAbsolute)
	{
		PUT_BYTE(temp, lb_value);
		PUT_STRING(temp, state->tokens[1]);
	}
	else if (!lhsIsAbsolute)
	{
		FREE_BUFFER(temp);
		state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Compare operation requires absolute type");
		return;
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

	if (state->tokencount > 2)
	{
		count = lb_two;
		byte_t comparatorByte = get_comparator_byte(state->tokens[2]);
		if (comparatorByte == 0)
		{
			FREE_BUFFER(temp);
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Unexpected token \"%s\", expected comparator", state->tokens[2]);
			return;
		}
		PUT_BYTE(temp, comparatorByte);

		if (state->tokencount < 4)
		{
			FREE_BUFFER(temp);
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Expected compare argument");
			return;
		}

		data_t rhsData;
		byte_t rhsType;
		int rhsIsAbsolute;
		size_t rhsSize = evaluate_constant(state->tokens[3], &rhsData, &rhsType, &rhsIsAbsolute);

		if (rhsSize == 0 && !rhsIsAbsolute)
		{
			PUT_BYTE(temp, lb_value);
			PUT_STRING(temp, state->tokens[3]);
		}
		else if (!rhsIsAbsolute)
		{
			FREE_BUFFER(temp);
			state->back = add_compile_error(state->back, state->srcfile, state->srcline, error_error, "Compare operation requires absolute type");
			return;
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

	PUT_BYTE(state->out, state->cmd);
	PUT_BYTE(state->out, count);
	PUT_BUF(state->out, temp);
	PUT_LONG(state->out, -1);
	FREE_BUFFER(temp);
}
