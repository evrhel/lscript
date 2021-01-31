#include "linker.h"

#include "buffer.h"
#include <stdio.h>
#include <internal/lb.h>

enum
{
	search_end = 0x1,
	search_else = 0x2,
};

static compile_error_t *link_file(const char *file, compile_error_t *back, unsigned int linkVersion);
static compile_error_t *link_data(byte_t *data, size_t datalen, compile_error_t *back, unsigned int linkVersion);

static byte_t *seek_to_next_control(byte_t *off, byte_t *end);
static byte_t *seek_to_cooresponding_end(byte_t *off, byte_t *end, size_t **linkStart, int searchType);
static byte_t *seek_past_if_style_cmd(byte_t *start, size_t **linkStart);

compile_error_t *link(input_file_t *files, unsigned int linkVersion, msg_func_t messenger)
{
	compile_error_t *errors = create_base_compile_error(messenger);
	compile_error_t *back = errors;

	if (linkVersion != 1)
		return add_compile_error(errors, "", 0, error_error, "Unsupported link standard.");

	while (files)
	{
		back = link_file(files->filename, back, linkVersion);
		files = files->next;
	}

	return errors;
}

compile_error_t *link_file(const char *filepath, compile_error_t *back, unsigned int linkVersion)
{
	FILE *file = NULL;
	fopen_s(&file, filepath, "rb");
	if (!file)
		return add_compile_error(back, filepath, 0, error_error, "Failed to fopen for read");

	fseek(file, 0, SEEK_END);
	long len = ftell(file);
	fseek(file, 0, SEEK_SET);

	byte_t *data = (byte_t *)malloc(len);
	if (!data)
		return add_compile_error(back, filepath, 0, error_error, "Failed to allocate buffer");

	fread_s(data, len, sizeof(byte_t), len, file);
	fclose(file);

	back = link_data(data, len, back, linkVersion);

	fopen_s(&file, filepath, "wb");
	if (!file)
		return add_compile_error(back, filepath, 0, error_error, "Failed to fopen for write");

	fwrite(data, sizeof(byte_t), len, file);

	fclose(file);

	free(data);

	return back;
}

compile_error_t *link_data(byte_t *data, size_t len, compile_error_t *back, unsigned int linkVersion)
{
	byte_t *counter = data;
	byte_t *end = data + len;
	size_t *linkLoc;
	byte_t *controlEnd;
	while (1)
	{
		counter = seek_to_next_control(counter, end);
		if (counter)
		{
			switch (*counter)
			{
			case lb_else:
				counter++;
				if (*counter != lb_if)
					break;
			case lb_if:
				controlEnd = seek_to_cooresponding_end(counter, end, &linkLoc, search_end | search_else);
				controlEnd++;
				*linkLoc = (size_t)(controlEnd - data);
				break;
			case lb_while:
				controlEnd = seek_to_cooresponding_end(counter, end, &linkLoc, search_end);
				controlEnd++;
				*linkLoc = (size_t)(controlEnd - data);
				break;
			}
		}
		else break;
	}
	return back;
}

byte_t *seek_to_next_control(byte_t *off, byte_t *end)
{
	unsigned char i;
	unsigned char argCount;
	while (off < end)
	{
		switch (*off)
		{
		case lb_if:
		case lb_else:
		case lb_while:
		case lb_end:
			return off;
			break;

		case lb_global:
			break;
		case lb_function:
			break;

		case lb_noop:
			off++;
			break;
		case lb_char:
		case lb_uchar:
		case lb_short:
		case lb_ushort:
		case lb_int:
		case lb_uint:
		case lb_ulong:
		case lb_bool:
		case lb_float:
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
			off++;
			off += strlen(off) + 1;
			break;

		case lb_setb:
			off += strlen(off) + 1;
			off += sizeof(byte_t);
			break;
		case lb_setw:
			off += strlen(off) + 1;
			off += sizeof(word_t);
			break;
		case lb_setd:
		case lb_setr4:
			off += strlen(off) + 1;
			off += sizeof(dword_t);
			break;
		case lb_setq:
		case lb_setr8:
			off += strlen(off) + 1;
			off += sizeof(qword_t);
			break;
		case lb_seto:
			off++;
			off += strlen(off) + 1;
			setObject:
			switch (*off)
			{
			case lb_new:
				off += strlen(off) + 1;	// Classname
				off += strlen(off) + 1;	// Constructor signature
				// Custom constructors not supported yet :'(
				break;
			case lb_value:
				off += strlen(off) + 1;
				break;
			case lb_string:
				off += strlen(off) + 1;
				break;
			case lb_null:
				off++;
				break;
			}
			break;
		case lb_setv:
			off += strlen(off) + 1;
			off += strlen(off) + 1;
			break;
		case lb_setr:
			off++;
			break;

		case lb_ret:
		case lb_retr:
			off++;
			break;
		case lb_retb:
			off++;
			off += sizeof(byte_t);
			break;
		case lb_retw:
			off++;
			off += sizeof(word_t);
			break;
		case lb_retr4:
		case lb_retd:
			off++;
			off += sizeof(dword_t);
			break;
		case lb_retr8:
		case lb_retq:
			off++;
			off += sizeof(qword_t);
			break;
		case lb_reto:
			off++;
			goto setObject;
			break;
		case lb_retv:
			off++;
			off += strlen(off) + 1;
			break;

		case lb_static_call:
		case lb_dynamic_call:
			off++;
			off += strlen(off) + 1;	// Function name
			argCount = *off;
			for (i = 0; i < argCount; i++)
			{
				switch (*off)
				{
				case lb_char:
				case lb_uchar:
				case lb_short:
				case lb_ushort:
				case lb_int:
				case lb_uint:
				case lb_long:
				case lb_ulong:
				case lb_real4:
				case lb_real8:
				case lb_bool:
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
					off++;
					off += strlen(off) + 1;	// Argument name
					break;
				case lb_object:
				case lb_objectarray:
					off++;
					off += strlen(off) + 1;	// Classname
					off += strlen(off) + 1;	// Argument name
					break;
				}
			}
			break;
		
		case lb_add:
		case lb_sub:
		case lb_mul:
		case lb_div:
		case lb_mod:
			off++;
			off += strlen(off) + 1;	// Destination variable name
			off += strlen(off) + 1;	// Source variable name
			switch (*off)			// Argument
			{
			case lb_char:
			case lb_uchar:
				off += sizeof(byte_t);
				break;
			case lb_short:
			case lb_ushort:
				off += sizeof(word_t);
				break;
			case lb_int:
			case lb_uint:
			case lb_real4:
				off += sizeof(dword_t);
				break;
			case lb_long:
			case lb_ulong:
			case lb_real8:
				off += sizeof(qword_t);
				break;
			}
			break;
		}
	}
	return off;
}

byte_t *seek_to_cooresponding_end(byte_t *off, byte_t *end, size_t **linkStart, int searchType)
{
	int level = 0;
	while (off && off < end)
	{
		switch (*off)
		{
		case lb_else:
			off++;
			if (*off != lb_if)
				break;
		case lb_if:
		case lb_while:
			level++;
			off = seek_past_if_style_cmd(off, level == 1 ? linkStart : NULL);
			break;
		case lb_end:
			level--;
			if (level == 0)
				return off;
			break;
		default:
			off = seek_to_next_control(off, end);
			if (!off)
			{
				if (linkStart)
					*linkStart = NULL;
				return NULL;
			}
			break;
		}
	}
	if (linkStart)
		*linkStart = NULL;
	return NULL;
}

byte_t *seek_past_if_style_cmd(byte_t *start, size_t **linkStart)
{
	byte_t *off = start;
	off++;
	byte_t count = *off;
	off++;

	switch (*off)
	{
	case lb_char:
	case lb_uchar:
		off++;
		off += sizeof(byte_t);
		break;
	case lb_short:
	case lb_ushort:
		off++;
		off += sizeof(word_t);
		break;
	case lb_int:
	case lb_uint:
	case lb_float:
		off += sizeof(dword_t);
		break;
	case lb_long:
	case lb_ulong:
	case lb_double:
		off += sizeof(qword_t);
		break;
	case lb_value:
		off++;
		off += strlen(off) + 1;
		break;
	default:
		break;
	}

	if (count == lb_one)
	{
		if (linkStart)
			*linkStart = (size_t *)off;
		off += sizeof(size_t);
	}
	else if (count = lb_two)
	{
		switch (*off)
		{
		case lb_char:
		case lb_uchar:
			off++;
			off += sizeof(byte_t);
			break;
		case lb_short:
		case lb_ushort:
			off++;
			off += sizeof(word_t);
			break;
		case lb_int:
		case lb_uint:
		case lb_float:
			off += sizeof(dword_t);
			break;
		case lb_long:
		case lb_ulong:
		case lb_double:
			off += sizeof(qword_t);
			break;
		case lb_value:
			off++;
			off += strlen(off) + 1;
			break;
		default:
			break;
		}

		if (linkStart)
			*linkStart = (size_t *)off;
		off += sizeof(size_t);
	}
	else
	{
		// bad
	}
	return off;
}
