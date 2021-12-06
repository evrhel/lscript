#include "dump.h"

#include <stdlib.h>
#include <assert.h>
#include <internal/lb.h>
#include <internal/types.h>
#include <internal/value.h>

#define printbyte(out, byte) fprintf(out, "[%02hhX]", byte)
#define printbytespc(out, byte) fprintf(out, "[%02hhX] ", byte)

typedef struct disasm_state_s
{
	FILE *out;
	byte_t *cursor;
	const char *lastfunc;
} disasm_state_t;

static int disasm(FILE *out, byte_t *data, long datalen);
static int expsyb(FILE *out, byte_t *data, long datalen);
static const char *type_name(byte_t lb);

static int determine_arg_count(const char *funcname);

/*
Handles a generic function call. state->cursor should point to the function signature.
*/
static void print_function_call_generic(disasm_state_t *state);

static void print_absolute_value(disasm_state_t *state);

static void print_function(disasm_state_t *state);
static void print_global(disasm_state_t *state);
static void print_valdef(disasm_state_t *state);
static void print_setcmd(disasm_state_t *state);
static void print_retcmd(disasm_state_t *state);
static void handle_ifstyle(disasm_state_t *state);
static void print_castcmd(disasm_state_t *state);

int dump_file(dump_options_t *dumpOptions)
{
	FILE *in = NULL, *out = NULL;
	long datalen = 0;
	byte_t *data = NULL;
	int err = dump_no_error;

	fopen_s(&in, dumpOptions->infile, "rb");
	if (!in) return dump_io_error;

	fseek(in, 0, SEEK_END);
	datalen = ftell(in);
	fseek(in, 0, SEEK_SET);

	data = (byte_t *)malloc(datalen);
	if (!data)
	{
		err = dump_out_of_memory;
		goto finalize;
	}

	fread_s(data, datalen, sizeof(byte_t), datalen, in);

	fclose(in);
	in = NULL;

	if (dumpOptions->outfile)
		fopen_s(&out, dumpOptions->outfile, "w");
	else
		out = stdout;

	if (dumpOptions->disasm)
	{
		err = disasm(out, data, datalen);
		if (err) goto finalize;
	}

	if (dumpOptions->symb)
	{
		err = expsyb(out, data, datalen);
		if (err) goto finalize;
	}

finalize:

	if (in) fclose(in);
	if (out && out != stdout) fclose(out);

	return err;
}

int disasm(FILE *out, byte_t *data, long datalen)
{
	disasm_state_t state;
	byte_t *end = NULL;
	const char *cmdname = NULL;
	size_t off;
	unsigned int version;
	char compressed;

	state.out = out;
	state.cursor = data;
	state.lastfunc = NULL;

	end = data + datalen;

	// compressed
	compressed = *state.cursor;
	state.cursor++;

	// version
	version = *state.cursor;
	state.cursor += sizeof(unsigned int);

	fprintf(out, "Bytecode version: %u\n", version);
	fprintf(out, "Compressed: %s\n", compressed ? "true" : "false");
	fprintf(out, "Size: %ld B\n\n", datalen);

	while (state.cursor < end)
	{
		off = state.cursor - data;
#if defined(_WIN32)
#if defined (_WIN64)
		fprintf(out, "0x%016zX: ", off);
#else
		fprintf(out, "0x%08zX: ", off);
#endif
#endif
		switch (*state.cursor)
		{
		case lb_noop:
			state.cursor++;
			fprintf(out, "noop");
			break;
		case lb_class:
			state.cursor++;
			fprintf(out, "class %s extends ", state.cursor);
			state.cursor += strlen(state.cursor) + 1;
			if (*state.cursor == lb_extends)
			{
				state.cursor++;
				fprintf(out, "%s", state.cursor);
				state.cursor += strlen(state.cursor) + 1;
			}
			else fprintf(out, "null");
			break;
		case lb_function:
			print_function(&state);
			break;
		case lb_global:
			print_global(&state);
			break;
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
		case lb_chararray:
		case lb_uchararray:
		case lb_shortarray:
		case lb_ushortarray:
		case lb_intarray:
		case lb_uintarray:
		case lb_longarray:
		case lb_ulongarray:
		case lb_boolarray:
		case lb_floatarray:
		case lb_doublearray:
		case lb_objectarray:
			print_valdef(&state);
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
			print_setcmd(&state);
			break;
		case lb_retb:
		case lb_retw:
		case lb_retd:
		case lb_retq:
		case lb_retr4:
		case lb_retr8:
		case lb_retv:
		case lb_ret:
		case lb_retr:
			print_retcmd(&state);
			break;
		case lb_static_call:
			fprintf(state.out, "static_call ");
			state.cursor++;
			print_function_call_generic(&state);
			break;
		case lb_dynamic_call:
			fprintf(state.out, "dynamic_call ");
			state.cursor++;
			print_function_call_generic(&state);
			break;
		case lb_add:
			cmdname = "add";
			goto handle_math_cmd;
			break;
		case lb_sub:
			cmdname = "sub";
			goto handle_math_cmd;
			break;
		case lb_mul:
			cmdname = "mul";
			goto handle_math_cmd;
			break;
		case lb_div:
			cmdname = "div";
			goto handle_math_cmd;
			break;
		case lb_mod:
			cmdname = "mod";
			goto handle_math_cmd;
			break;
		case lb_and:
			cmdname = "and";
			goto handle_math_cmd;
			break;
		case lb_or:
			cmdname = "or";
			goto handle_math_cmd;
			break;
		case lb_xor:
			cmdname = "xor";
			goto handle_math_cmd;
			break;
		case lb_lsh:
			cmdname = "lsh";
			goto handle_math_cmd;
			break;
		case lb_rsh:
			cmdname = "rsh";
		handle_math_cmd:
			state.cursor++;
			fprintf(state.out, "%s %s ", cmdname, state.cursor); // cmd dest
			state.cursor += strlen(state.cursor) + 1;
			fprintf(state.out, "%s ", state.cursor); // src
			state.cursor += strlen(state.cursor) + 1;
			print_absolute_value(&state);
			break;
		case lb_neg:
			cmdname = "neg";
			goto handle_unary_math_cmd;
		case lb_not:
			cmdname = "not";
		handle_unary_math_cmd:
			state.cursor++;
			fprintf(state.out, "%s %s ", cmdname, state.cursor); // cmd dest
			state.cursor += strlen(state.cursor) + 1;
			fprintf(state.out, "%s ", state.cursor); // src
			state.cursor += strlen(state.cursor) + 1;
			break;
		case lb_elif:
			state.cursor++;
			fprintf(state.out, "elif (%llX) ", *(qword_t *)state.cursor);
			state.cursor += sizeof(qword_t);
			goto handle_if_style_cmd;
		case lb_while:
			cmdname = "while";
			state.lastfunc = NULL;
			goto handle_if_style_cmd;
		case lb_if:
			cmdname = "if";
		handle_if_style_cmd:
			handle_ifstyle(&state);
			break;
		case lb_else:
			cmdname = "else";
			goto handle_end_style_cmd;
		case lb_end:
			cmdname = "end";
		handle_end_style_cmd:
			state.cursor++;
			fprintf(state.out, "%s (0x%016llX)", cmdname, *((qword_t *)state.cursor));
			state.cursor += sizeof(qword_t);
			state.lastfunc = NULL;
			break;
		case lb_align:
			fprintf(state.out, "align");
			while (*state.cursor == lb_align) state.cursor++;
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
			print_castcmd(&state);
			break;
		default:
			printbyte(out, *state.cursor);
			state.cursor++;
			break;
		}

		fprintf(out, "\n");
	}

	return dump_no_error;
}

int expsyb(FILE *out, byte_t *data, long datalen)
{
	return dump_not_supported;
}

const char *type_name(byte_t lb)
{
	switch (lb)
	{
	case lb_void:
		return "void";
	case lb_char:
		return "char";
	case lb_uchar:
		return "uchar";
	case lb_short:
		return "short";
	case lb_ushort:
		return "ushort";
	case lb_int:
		return "int";
	case lb_uint:
		return "uint";
	case lb_long:
		return "long";
	case lb_ulong:
		return "ulong";
	case lb_bool:
		return "bool";
	case lb_float:
		return "float";
	case lb_double:
		return "double";
	case lb_object:
		return "object";
	case lb_chararray:
		return "chararray";
	case lb_uchararray:
		return "uchararray";
	case lb_shortarray:
		return "shortarray";
	case lb_ushortarray:
		return "ushortarray";
	case lb_intarray:
		return "intarray";
	case lb_uintarray:
		return "uintarray";
	case lb_longarray:
		return "longarray";
	case lb_ulongarray:
		return "ulongarray";
	case lb_boolarray:
		return "boolarray";
	case lb_floatarray:
		return "floatarray";
	case lb_doublearray:
		return "doublearray";
	case lb_objectarray:
		return "objectarray";
	case lb_byte:
		return "byte";
	case lb_word:
		return "word";
	case lb_dword:
		return "dword";
	case lb_qword:
		return "qword";
	case lb_real4:
		return "real4";
	case lb_real8:
		return "real8";
	default:
		return NULL;
	}
}

int determine_arg_count(const char *funcname)
{
	const char *cursor = funcname;
	int count = 0;
	while (*cursor && *cursor != '(') cursor++;

	if (!*cursor) return 0;
	cursor++;

	for (; *cursor; cursor++)
	{
		if (*cursor == '[') continue;
		if (*cursor == 'L')
		{
			while (*cursor && *cursor != ';') cursor++;
		}
		count++;
	}
	return count;
}

void print_function_call_generic(disasm_state_t *state)
{
	int i, argc;
	byte_t datatype;

	state->lastfunc = state->cursor;

	fprintf(state->out, "%s)", state->cursor);
	argc = determine_arg_count(state->cursor);

	state->cursor += strlen(state->cursor) + 1;
	for (i = 0; i < argc; i++)
	{
		fputc(' ', state->out);
		datatype = *state->cursor;
		state->cursor++;
		switch (datatype)
		{
		case lb_byte:
			fprintf(state->out, "byte[0x%hhX]", *((byte_t *)state->cursor));
			state->cursor += sizeof(byte_t);
			break;
		case lb_word:
			fprintf(state->out, "word[0x%hX]", *((word_t *)state->cursor));
			state->cursor += sizeof(word_t);
			break;
		case lb_dword:
			fprintf(state->out, "dword[0x%X]", *((dword_t *)state->cursor));
			state->cursor += sizeof(dword_t);
			break;
		case lb_qword:
			fprintf(state->out, "qword[0x%llX]", *((qword_t *)state->cursor));
			state->cursor += sizeof(qword_t);
			break;
		case lb_real4:
			fprintf(state->out, "real4[%g]", (real8_t) * ((real4_t *)state->cursor));
			state->cursor += sizeof(real4_t);
			break;
		case lb_real8:
			fprintf(state->out, "real8[%g]", *((real8_t *)state->cursor));
			state->cursor += sizeof(real8_t);
			break;
		case lb_value:
			fprintf(state->out, "%s", state->cursor);
			state->cursor += strlen(state->cursor) + 1;
			break;
		case lb_string:
			fprintf(state->out, "\"%s\"", state->cursor);
			state->cursor += strlen(state->cursor) + 1;
			break;
		case lb_ret:
			fprintf(state->out, "[ret]");
			state->cursor++;
			break;
		default:
			printbyte(state->out, *state->cursor);
			state->cursor++;
			break;
		}
	}
}

void print_absolute_value(disasm_state_t *state)
{
	byte_t argtype = *state->cursor;
	state->cursor++;
	switch (argtype)
	{
	case lb_char:
		fprintf(state->out, "char[%hhd]", *((lchar *)state->cursor));
		state->cursor += sizeof(lchar);
		break;
	case lb_uchar:
		fprintf(state->out, "uchar[%hhu]", *((luchar *)state->cursor));
		state->cursor += sizeof(luchar);
		break;
	case lb_short:
		fprintf(state->out, "uchar[%hd]", *((lshort *)state->cursor));
		state->cursor += sizeof(lshort);
		break;
	case lb_ushort:
		fprintf(state->out, "uchar[%hu]", *((lushort *)state->cursor));
		state->cursor += sizeof(lushort);
		break;
	case lb_int:
		fprintf(state->out, "int[%d]", *((lint *)state->cursor));
		state->cursor += sizeof(lint);
		break;
	case lb_uint:
		fprintf(state->out, "uint[%u]", *((luint *)state->cursor));
		state->cursor += sizeof(luint);
		break;
	case lb_long:
		fprintf(state->out, "long[%lld]", *((llong *)state->cursor));
		state->cursor += sizeof(llong);
		break;
	case lb_ulong:
		fprintf(state->out, "ulong[%llu]", *((lulong *)state->cursor));
		state->cursor += sizeof(lulong);
		break;
	case lb_float:
		fprintf(state->out, "float[%g]", (ldouble) * ((lfloat *)state->cursor));
		state->cursor += sizeof(lfloat);
		break;
	case lb_double:
		fprintf(state->out, "double[%g]", *((ldouble *)state->cursor));
		state->cursor += sizeof(ldouble);
		break;
	case lb_value:
		fprintf(state->out, "%s", state->cursor);
		state->cursor += strlen(state->cursor) + 1;
		break;
	}
}

void print_function(disasm_state_t *state)
{
	const char *dataTypeName, *classname;
	byte_t argcount, i;
	byte_t type;

	fprintf(state->out, "function ");
	state->cursor++;

	if (*state->cursor == lb_static)
		fprintf(state->out, "static ");
	else if (*state->cursor == lb_dynamic)
		fprintf(state->out, "dynamic ");
	else
		printbytespc(state->out, *state->cursor);
	state->cursor++;

	if (*state->cursor == lb_interp)
		fprintf(state->out, "interp ");
	else if (*state->cursor == lb_native)
		fprintf(state->out, "native ");
	else if (*state->cursor == lb_abstract)
		fprintf(state->out, "abstract ");
	else
		printbytespc(state->out, *state->cursor);
	state->cursor++;

	dataTypeName = type_name(*state->cursor);
	if (dataTypeName)
		fprintf(state->out, "%s ", dataTypeName);
	else
		printbytespc(state->out, *state->cursor);
	state->cursor++;

	fprintf(state->out, "%s(", state->cursor);
	state->cursor += strlen(state->cursor) + 1;

	argcount = *state->cursor;
	state->cursor++;

	for (i = 0; i < argcount; i++)
	{
		type = *state->cursor;
		state->cursor++;

		dataTypeName = type_name(type);
		if (!dataTypeName)
			printbytespc(state->out, type);
		else
		{
			fprintf(state->out, "%s ", dataTypeName);
			if (type == lb_object || type == lb_objectarray)
			{
				classname = state->cursor;
				state->cursor += strlen(state->cursor) + 1;
				fprintf(state->out, "%s ", classname);
			}
		}

		// arg name
		fprintf(state->out, "%s", state->cursor);
		state->cursor += strlen(state->cursor) + 1;

		if (i < argcount - 1)
			fprintf(state->out, ", ");
	}

	fprintf(state->out, ")");
}

void print_global(disasm_state_t *state)
{
	const char *name, *typeName;
	value_t *value;
	size_t size;
	byte_t accesstype, accessmodifier, datatype;

	state->cursor++;

	name = state->cursor;
	state->cursor += strlen(state->cursor) + 1;

	value = (value_t *)state->cursor;
	state->cursor += sizeof(flags_t); // Don't increment by sizeof(flags_t), there might not be a value stored

	accesstype = value_access_type(value);
	accessmodifier = value_access_modifier(value);
	datatype = value_typeof(value);

	fprintf(state->out, "global ");

	if (accesstype == lb_dynamic)
		fprintf(state->out, "dynamic ");
	else if (accesstype == lb_static)
		fprintf(state->out, "static ");
	else
		printbytespc(state->out, accesstype);

	if (accessmodifier == lb_varying)
		fprintf(state->out, "varying ");
	else if (accessmodifier == lb_const)
		fprintf(state->out, "const ");
	else
		printbytespc(state->out, accessmodifier);

	typeName = type_name(datatype);
	if (typeName)
		fprintf(state->out, "%s ", typeName);
	else
		printbytespc(state->out, datatype);

	fprintf(state->out, "%s", name);

	if (accesstype == lb_static)
	{
		fputc(' ', state->out);
		if (datatype == lb_float)
		{
			fprintf(state->out, "real4[%g]", (real8_t)*((real4_t *)state->cursor));
			state->cursor += sizeof(real8_t);
		}
		else if (datatype == lb_double)
		{
			fprintf(state->out, "real8[%g]", *((real8_t *)state->cursor));
			state->cursor += sizeof(real4_t);
		}
		else
		{
			size = sizeof_type(datatype);
			switch (size)
			{
			case 1:
				fprintf(state->out, "byte[0x%hhX]", *((byte_t *)state->cursor));
				break;
			case 2:
				fprintf(state->out, "word[0x%hX]", *((word_t *)state->cursor));
				break;
			case 4:
				fprintf(state->out, "dword[0x%X]", *((dword_t *)state->cursor));
				break;
			case 8:
				fprintf(state->out, "qword[0x%llX]", *((qword_t *)state->cursor));
				break;
			default:
				fprintf(state->out, "?");
			}
			state->cursor += size;
		}
	}
	
	state->lastfunc = NULL;
}

void print_valdef(disasm_state_t *state)
{
	const char *typeName, *valName;

	typeName = type_name(*state->cursor);
	assert(typeName != NULL);
	state->cursor++;

	valName = state->cursor;
	state->cursor += strlen(state->cursor) + 1;
	
	fprintf(state->out, "%s %s", typeName, valName);
}

void print_setcmd(disasm_state_t *state)
{
	byte_t setcmd;
	const char *destvar, *srcvar;
	const char *arrtype;

	setcmd = *state->cursor;

	state->cursor++;
	destvar = state->cursor;
	state->cursor += strlen(state->cursor) + 1;

	switch (setcmd)
	{
	case lb_setb:
		fprintf(state->out, "setb %s byte[0x%hhX]", destvar, *((byte_t *)state->cursor));
		state->cursor += sizeof(byte_t);
		break;
	case lb_setw:
		fprintf(state->out, "setw %s word[0x%hX]", destvar, *((word_t *)state->cursor));
		state->cursor += sizeof(word_t);
		break;
	case lb_setd:
		fprintf(state->out, "setd %s dword[0x%X]", destvar, *((dword_t *)state->cursor));
		state->cursor += sizeof(dword_t);
		break;
	case lb_setq:
		fprintf(state->out, "setq %s qword[0x%llX]", destvar, *((qword_t *)state->cursor));
		state->cursor += sizeof(qword_t);
		break;
	case lb_setr4:
		fprintf(state->out, "setr4 %s real4[%g]", destvar, (real8_t)*((real4_t *)state->cursor));
		state->cursor += sizeof(real4_t);
		break;
	case lb_setr8:
		fprintf(state->out, "sett8 %s real8[%g]", destvar, *((real8_t *)state->cursor));
		state->cursor += sizeof(real8_t);
		break;
	case lb_seto:
		fprintf(state->out, "seto %s ", destvar);
		switch (*state->cursor)
		{
		case lb_char:
			arrtype = "char";
			goto handle_new_array;
		case lb_uchar:
			arrtype = "uchar";
			goto handle_new_array;
		case lb_short:
			arrtype = "short";
			goto handle_new_array;
		case lb_ushort:
			arrtype = "ushort";
			goto handle_new_array;
		case lb_int:
			arrtype = "int";
			goto handle_new_array;
		case lb_uint:
			arrtype = "uint";
			goto handle_new_array;
		case lb_long:
			arrtype = "long";
			goto handle_new_array;
		case lb_ulong:
			arrtype = "ulong";
			goto handle_new_array;
		case lb_bool:
			arrtype = "bool";
			goto handle_new_array;
		case lb_float:
			arrtype = "float";
			goto handle_new_array;
		case lb_double:
			arrtype = "double";
			goto handle_new_array;
		case lb_object:
			arrtype = "object";
		handle_new_array:
			state->cursor++;
			fprintf(state->out, "%s ", arrtype);
			switch (*state->cursor)
			{
			case lb_value:
				state->cursor++;
				fprintf(state->out, "%s", state->cursor);
				state->cursor += strlen(state->cursor) + 1;
				break;
			case lb_dword:
				state->cursor++;
				fprintf(state->out, "dword[%u]", *((dword_t *)state->cursor));
				state->cursor += sizeof(dword_t);
				break;
			default:
				printbyte(state->out, *state->cursor);
				state->cursor++;
				break;
			}
			break;
		case lb_new:
			state->cursor++;
			fprintf(state->out, "new %s ", state->cursor);
			state->cursor += strlen(state->cursor) + 1;

			print_function_call_generic(state);
			break;
		case lb_string:
			state->cursor++;
			fprintf(state->out, "\"%s\"", state->cursor);
			state->cursor += strlen(state->cursor) + 1;
			break;
		case lb_null:
			fprintf(state->out, "null");
			state->cursor++;
			break;
		default:
			printbyte(state->out, *state->cursor);
			state->cursor++;
			break;
		}
		break;
	case lb_setv:
		srcvar = state->cursor;
		state->cursor += strlen(state->cursor) + 1;
		fprintf(state->out, "setv %s %s", destvar, srcvar);
		break;
	case lb_setr:
		fprintf(state->out, "setr %s (%s)", destvar, state->lastfunc ? state->lastfunc : "???");
		break;
	}
}

void print_retcmd(disasm_state_t *state)
{
	byte_t retcmd;

	retcmd = *state->cursor;
	state->cursor++;

	switch (retcmd)
	{
	case lb_retb:
		fprintf(state->out, "retb byte[0x%hhX]",*((byte_t *)state->cursor));
		state->cursor += sizeof(byte_t);
		break;
	case lb_retw:
		fprintf(state->out, "retw word[0x%hX]", *((word_t *)state->cursor));
		state->cursor += sizeof(word_t);
		break;
	case lb_retd:
		fprintf(state->out, "retd dword[0x%X]", *((dword_t *)state->cursor));
		state->cursor += sizeof(dword_t);
		break;
	case lb_retq:
		fprintf(state->out, "retq qword[0x%llX]",  *((qword_t *)state->cursor));
		state->cursor += sizeof(qword_t);
		break;
	case lb_retr4:
		fprintf(state->out, "retr4 real4[%g]", (real8_t)*((real4_t *)state->cursor));
		state->cursor += sizeof(real4_t);
		break;
	case lb_retr8:
		fprintf(state->out, "rett8 real8[%g]", *((real8_t *)state->cursor));
		state->cursor += sizeof(real8_t);
		break;
	case lb_ret:
		fprintf(state->out, "ret");
		break;
	case lb_retr:
		fprintf(state->out, "retr (%s)", state->lastfunc ? state->lastfunc : "???");
		break;
	case lb_retv:
		fprintf(state->out, "retv %s", state->cursor);
		state->cursor += strlen(state->cursor) + 1;
		break;
	}

	state->lastfunc = NULL;
}

void handle_ifstyle(disasm_state_t *state)
{
	byte_t count;
	byte_t comparator;
	qword_t offset;

	switch (*state->cursor)
	{
	case lb_while:
		fprintf(state->out, "while ");
		break;
	case lb_if:
		fprintf(state->out, "if ");
		break;
	default:
		assert(0);
		break;
	}
	state->cursor++;
	
	count = *state->cursor;
	state->cursor++;

	print_absolute_value(state);

	if (count == lb_two)
	{
		comparator = *state->cursor;
		state->cursor++;

		fputc(' ', state->out);
		switch (comparator)
		{
		case lb_equal:
			fprintf(state->out, "== ");
			break;
		case lb_nequal:
			fprintf(state->out, "!= ");
			break;
		case lb_greater:
			fprintf(state->out, "> ");
			break;
		case lb_less:
			fprintf(state->out, "< ");
			break;
		case lb_lequal:
			fprintf(state->out, "<= ");
			break;
		default:
			fprintf(state->out, "? ");
			break;
		}

		print_absolute_value(state);
	}
	else if (count != lb_one) return;

	fprintf(state->out, " (0x%016llX)", *((qword_t *)state->cursor));
	state->cursor += sizeof(qword_t);
}

void print_castcmd(disasm_state_t *state)
{
	byte_t cmd;

	cmd = *state->cursor;
	state->cursor++;
	switch (cmd)
	{
	case lb_castc:
		fprintf(state->out, "castc ");
		break;
	case lb_castuc:
		fprintf(state->out, "castuv ");
		break;
	case lb_casts:
		fprintf(state->out, "casts ");
		break;
	case lb_castus:
		fprintf(state->out, "castus ");
		break;
	case lb_casti:
		fprintf(state->out, "casti ");
		break;
	case lb_castui:
		fprintf(state->out, "castui ");
		break;
	case lb_castl:
		fprintf(state->out, "castl ");
		break;
	case lb_castul:
		fprintf(state->out, "castul ");
		break;
	case lb_castb:
		fprintf(state->out, "castb ");
		break;
	case lb_castf:
		fprintf(state->out, "castf ");
		break;
	case lb_castd:
		fprintf(state->out, "castd ");
		break;
	default:
		assert(0);
		fprintf(state->out, "?");
		return;
	}

	fprintf(state->out, "%s ", state->cursor); // cmd dest
	state->cursor += strlen(state->cursor) + 1;
	fprintf(state->out, "%s ", state->cursor); // src
	state->cursor += strlen(state->cursor) + 1;
}
