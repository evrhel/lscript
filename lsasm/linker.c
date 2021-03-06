#include "linker.h"

#include "buffer.h"
#include <stdio.h>
#include <internal/value.h>
#include <assert.h>

enum
{
	search_end = 0x1,		// We want to search for an end command
	search_else = 0x2,		// We want to search for an else or elif command
	search_no_link = 0x4,	// Don't link anything
	search_no_level = 0x8	// Not used
};

static compile_error_t *link_file(const char *file, compile_error_t *back, unsigned int linkVersion);
static compile_error_t *link_data(byte_t *data, size_t datalen, const char *srcFile, compile_error_t *back, unsigned int linkVersion);

static byte_t *seek_to_next_control(byte_t *off, byte_t *end, const char *srcFile, compile_error_t **backPtr);
static byte_t *link_if_cmd(byte_t *start, byte_t *off, byte_t *end, int searchType, const char *srcFile, compile_error_t **backPtr);
static byte_t *link_while_cmd(byte_t *start, byte_t *off, byte_t *end, int searchType, const char *srcFile, compile_error_t **backPtr);
static byte_t *seek_past_if_style_cmd(byte_t *start, size_t **linkStart, const char *srcFile, compile_error_t **backPtr);

static unsigned char infer_argument_count(const char *qualname);

compile_error_t *link(input_file_t *files, unsigned int linkVersion, msg_func_t messenger)
{
	compile_error_t *errors = create_base_compile_error(messenger);
	compile_error_t *back = errors;

	if (linkVersion != 1)
		return add_compile_error(errors, "", 0, error_error, "Unsupported link standard.");

	while (files)
	{
		back = link_file(files->fullpath, back, linkVersion);
		files = files->next;
	}

	return errors ? errors->front : NULL;
}

compile_error_t *link_file(const char *filepath, compile_error_t *back, unsigned int linkVersion)
{
	back = add_compile_error(back, NULL, 0, error_info, "Link: %s", filepath);

	FILE *file = NULL;
	fopen_s(&file, filepath, "rb");
	if (!file)
		return add_compile_error(back, filepath, 0, error_error, "Failed to fopen for read");

	fseek(file, 0, SEEK_END);
	long len = ftell(file);
	fseek(file, 0, SEEK_SET);

	byte_t *data = (byte_t *)MALLOC(len);
	if (!data)
		return add_compile_error(back, filepath, 0, error_error, "Failed to allocate buffer");

	fread_s(data, len, sizeof(byte_t), len, file);
	fclose(file);

	back = link_data(data, len, filepath, back, linkVersion);

	int hasWarnings = 0;
	if (back)
	{
		compile_error_t *curr = back->front;
		while (curr)
		{
			if (curr->type == error_error)
			{
				back = add_compile_error(back, NULL, 0, error_info, "Link error in file: %s", file);
				return back->front;
			}
			else if (curr->type == error_warning)
				hasWarnings = 1;
			curr = curr->next;
		}
	}

	fopen_s(&file, filepath, "wb");
	if (!file)
		return add_compile_error(back, filepath, 0, error_error, "Failed to fopen for write");

	fwrite(data, sizeof(byte_t), len, file);

	fclose(file);

	FREE(data);

	if (hasWarnings)
		back = add_compile_error(back, NULL, 0, error_info, "%s linked with warnings.", filepath);
	else
		back = add_compile_error(back, NULL, 0, error_info, "%s successfully linked.", filepath);

	return back;
}

compile_error_t *link_data(byte_t *data, size_t len, const char *srcFile, compile_error_t *back, unsigned int linkVersion)
{
	byte_t *counter = data;
	byte_t *end = data + len;
	//size_t *linkLoc;
	//byte_t *controlEnd;

	counter += 5; // (unused) compressed bit and version number

	if (*counter != lb_class)
		return add_compile_error(back, srcFile, 0, error_error, "Bad file for link");

	counter++; // class
	counter += strlen(counter) + 1; // classname;
	if (*counter == lb_extends)
	{
		counter++; // extends
		counter += strlen(counter) + 1; // superclass name
	}

	while (1)
	{
		counter = seek_to_next_control(counter, end, srcFile, &back);
		if (counter >= end)
			break;
		switch (*counter)
		{
		case lb_else:
			counter++;
			counter += sizeof(size_t);
			if (*counter != lb_if)
				break;
		case lb_if:
			counter = link_if_cmd(data, counter, end, search_end | search_else, srcFile, &back);
			break;
		case lb_while:
			counter = link_while_cmd(data, counter, end, search_end, srcFile, &back);
			break;
		case lb_elif:
			// Skip the elif statement, it should have already been linked
			counter++;
			counter += sizeof(size_t);
			counter = link_if_cmd(data, counter, end, search_end | search_else, srcFile, &back);
				// seek_past_if_style_cmd(counter, NULL, srcFile, &back);
			break;
		case lb_end:
			// Skip the end statement, it should have already been linked
			counter++;
			counter += sizeof(size_t);
			break;
		default:
			back = add_compile_error(back, srcFile, 0, error_error, "Failed to seek to a valid control statement.");
		case NULL:
			return back;
			break;
		}
	}
	return back;
}

byte_t *seek_to_next_control(byte_t *off, byte_t *end, const char *srcFile, compile_error_t **backPtr)
{
	unsigned char i;
	unsigned char argCount;
	value_t *val;
	while (off < end)
	{
		assert(*off != lb_class);

		switch (*off)
		{
		case lb_if:
		case lb_else:
		case lb_elif:
		case lb_while:
		case lb_end:
			return off;
			break;

		case lb_global:
			off++;
			off += strlen(off) + 1;
			val = (value_t *)off;
			off += 8; // flags field
			if (value_access_type(val) == lb_static)
			{
				off += value_sizeof(val);
			}
			break;
		case lb_function:
			off++;
			off += 3; // 2 function qualifiers and return type
			off += strlen(off) + 1; // function name;
			argCount = *off;
			off++;
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
				case lb_float:
				case lb_double:
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
				case lb_boolarray:
					off++;
					off += strlen(off) + 1; // argument name
					break;
				case lb_object:
				case lb_objectarray:
					off++;
					off += strlen(off) + 1; // argument classname
					off += strlen(off) + 1; // argument name
					break;
				}
			}
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
		case lb_long:
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
				argCount = infer_argument_count((char *)off);
				off += strlen(off) + 1;	// Constructor signature
				// Custom constructors not supported yet :'(
				for (i = 0; i < argCount; i++)
				{
					switch (*off)
					{
					case lb_byte:
						off++;
						off += sizeof(byte_t);
						break;
					case lb_word:
						off++;
						off += sizeof(word_t);
						break;
					case lb_dword:
						off++;
						off += sizeof(dword_t);
						break;
					case lb_qword:
						off++;
						off += sizeof(qword_t);
						break;
					case lb_string:
					case lb_value:
						off++;
						off += strlen(off) + 1;
						break;
					}
				}
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
			off += strlen(off) + 1;
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
			argCount = infer_argument_count((char *)off);
			off += strlen(off) + 1;	// Function name
			for (i = 0; i < argCount; i++)
			{
				switch (*off)
				{
				case lb_byte:
					off++;
					off += sizeof(byte_t);
					break;
				case lb_word:
					off++;
					off += sizeof(word_t);
					break;
				case lb_dword:
					off++;
					off += sizeof(dword_t);
					break;
				case lb_qword:
					off++;
					off += sizeof(qword_t);
					break;
				case lb_string:
				case lb_value:
					off++;
					off += strlen(off) + 1;
					break;
				}
			}
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
			case lb_value:
				off++;
				off += strlen(off) + 1;
				break;
			}
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
		case lb_neg:
		case lb_not:
			// Handle unary operators

			off++;
			off += strlen(off) + 1;	// Destination variable name
			off += strlen(off) + 1;	// Source variable name
			break;

		case lb_align:
			off++;
			break;

		default:
			assert(0);
			return NULL;
			//off++; // This is potentially dangerous, but is here to skip over alignment bytes
			break;
		}
	}
	return off;
}

byte_t *link_if_cmd(byte_t *start, byte_t *off, byte_t *end, int searchType, const char *srcFile, compile_error_t **backPtr)
{
	int level = 0;				// The control level we are in (start with while or if, end with end)
	byte_t *top = off;			// The top of the if command
	byte_t *failLoc = NULL;		// The relative location in bytecode where the program will jump to on comparison fail
	byte_t *exitLoc = NULL;		// The relative location in bytecode where the program will jump to no matter what
	size_t *failLinkLoc = NULL;	// The location in bytecode where the relative address when comparison fails is stored
	size_t *exitLinkLoc = NULL;	// The location in bytecode where the relative address to exit the whole control block is stored
	while (off && off < end)
	{
		switch (*off)
		{
		case lb_else:
			off++;
			if (searchType & search_else)
			{
				// When the instructions above this statement execute, exit the whole block
				exitLinkLoc = (size_t *)off;
				off += sizeof(size_t);
				failLoc = off;

				// Seek to the end of the whole control block
				exitLoc = link_if_cmd(start, off, end, search_end | search_no_link, srcFile, backPtr);
				if (searchType & search_no_link)
					return off;
				goto perform_if_link;
			}
			else off += sizeof(size_t);
			break;
		case lb_elif:
			off++;
			if (searchType & search_else)
			{
				// When the instructions above this statement execute, exit the whole block
				exitLinkLoc = (size_t *)off;
				off += sizeof(size_t);

				// Next should be if statement
				assert(*off == lb_if);

				// The location in bytecode where the vm will jump to on fail
				failLoc = off;

				// Seek to the end of the whole control block
				exitLoc = link_if_cmd(start, off, end, search_end | search_no_link, srcFile, backPtr);
				if (searchType & search_no_link)
					return off;
				goto perform_if_link;
			}
			else
			{
				off += sizeof(size_t);
				off = seek_past_if_style_cmd(off, NULL, srcFile, backPtr);
			}
		case lb_if:
		case lb_while:
			// Increment the control statement level we are in
			level++;

			// Seek past this if command
			off = seek_past_if_style_cmd(off, NULL, srcFile, backPtr);
			
			// If we are at base level, this is where the vm will jump to on fail
			if (level == 1)
				failLinkLoc = (size_t *)(off - sizeof(size_t));
			break;
		case lb_end:
			// Decrement the control statement level we are in
			level--;

			// If we reached the last end statement and we are searching, set vars
			if (level <= 0 && searchType & search_end)
			{
				// This is where 
				exitLoc = off;

				// If we don't have a fail location, this is where we fail to
				if (!failLoc)
					failLoc = off;

				if (searchType & search_no_link)
					return off;
				goto perform_if_link;
			}

			// Seek past this command
			off += 9; // opcode + 8-byte offset
			break;
		default:
			// Seek to next control statement
			off = seek_to_next_control(off, end, srcFile, backPtr);
			if (!off)
				return NULL;
			break;
		}
	}
perform_if_link:
	// Assign each location in the bytecode

	if (failLinkLoc && failLoc)
		*failLinkLoc = failLoc - start;
	
	if (exitLinkLoc && exitLoc)
		*exitLinkLoc = exitLoc - start;

	return (byte_t *)(failLinkLoc + 1); // Return start of next command
}

byte_t *link_while_cmd(byte_t *start, byte_t *off, byte_t *end, int searchType, const char *srcFile, compile_error_t **backPtr)
{
	int level = 0;
	byte_t *topLoc = off;
	byte_t *failLoc = NULL;
	size_t *topLinkLoc = NULL;
	size_t *failLinkLoc = NULL;

	while (off && off < end)
	{
		switch (*off)
		{
		case lb_else:
			if (level == 1)
			{
				*backPtr = add_compile_error(*backPtr, srcFile, 0, error_error, "Unexepected else command while linking while command.");
				return NULL;
			}

			off++;
			off += sizeof(size_t);
			break;
		case lb_elif:
			off++;
			off += sizeof(size_t);
			assert(*off == lb_if);
		case lb_if:
		case lb_while:
			level++;
			off = seek_past_if_style_cmd(off, NULL, srcFile, backPtr);
			if (level == 1)
				failLinkLoc = (size_t *)(off - sizeof(size_t));
			break;
		case lb_end:
			level--;
			off++;
			off += sizeof(size_t);
			if (level == 0 && searchType & search_end)
			{
				topLinkLoc = (size_t *)(off - sizeof(size_t));
				failLoc = off;
				if (searchType & search_no_link)
					return off;
				goto perform_while_link;
			}
			break;
		default:
			off = seek_to_next_control(off, end, srcFile, backPtr);
			if (!off)
				return NULL;
			break;
		}
	}
perform_while_link:

	if (!failLinkLoc)
	{
		*backPtr = add_compile_error(*backPtr, srcFile, 0, error_error, "Error linking while command: NULL fail link location");
		return NULL;
	}

	if (failLoc)
		*failLinkLoc = failLoc - start;

	if (!topLinkLoc)
	{
		*backPtr = add_compile_error(*backPtr, srcFile, 0, error_error, "Error linking while command: NULL top link location");
		return NULL;
	}

	if (topLoc)
		*topLinkLoc = topLoc - start;

	return (byte_t *)(failLinkLoc + 1); // Return start of next command
}

byte_t *seek_past_if_style_cmd(byte_t *start, size_t **linkStart, const char *srcFile, compile_error_t **backPtr)
{
	assert(*start == lb_if || *start == lb_while);

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
	else if (count == lb_two)
	{
		off++; // comparator
		switch (*off)
		{
		case lb_bool:
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
			off++;
			off += sizeof(dword_t);
			break;
		case lb_long:
		case lb_ulong:
		case lb_double:
			off++;
			off += sizeof(qword_t);
			break;
		case lb_value:
			off++;
			off += strlen(off) + 1;
			break;
		default:
			assert(0);
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

unsigned char infer_argument_count(const char *qualname)
{
	unsigned char argCount = 0;
	char *cursor = strchr(qualname, '(');
	if (!cursor)
		return 0;
	cursor++;
	while (*cursor)
	{
		if (*cursor == 'L')
			cursor = strchr(cursor, ';');
		cursor++;
		argCount++;
	}
	return argCount;
}
