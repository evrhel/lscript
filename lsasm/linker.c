#include "linker.h"

#include "buffer.h"
#include <stdio.h>
#include <internal/lb.h>

static compile_error_t *link_file(const char *file, compile_error_t *back, unsigned int linkVersion);
static compile_error_t *link_data(byte_t *data, size_t datalen, compile_error_t *back, unsigned int linkVersion);

static byte_t *seek_to_next_control(byte_t *off, byte_t *end);
static compile_error_t *handle_if_style_link(byte_t *data, compile_error_t *back);

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

compile_error_t *link_file(const char *file, compile_error_t *back, unsigned int linkVersion)
{
	FILE *file = NULL;
	fopen_s(&file, file, "rb");
	if (!file)
		return add_compile_error(back, file, 0, error_error, "Failed to fopen for read");

	fseek(file, 0, SEEK_END);
	long len = ftell(file);
	fseek(file, 0, SEEK_SET);

	byte_t *data = (byte_t *)malloc(len);
	if (!data)
		return add_compile_error(back, file, 0, error_error, "Failed to allocate buffer");

	fread_s(data, len, sizeof(byte_t), len, file);
	fclose(file);

	back = link_data(data, len, back, linkVersion);

	fopen_s(&file, file, "wb");
	if (!file)
		return add_compile_error(back, file, 0, error_error, "Failed to fopen for write");

	fwrite(data, sizeof(byte_t), len, file);

	fclose(file);

	free(data);

	return back;
}

compile_error_t *link_data(byte_t *data, size_t len, compile_error_t *back, unsigned int linkVersion)
{
	byte_t *counter = data;
	byte_t *end = data + len;
	while (counter < end)
	{
		switch (*counter)
		{
		case lb_if:
		case lb_while:
			break;
		case lb_else:
			break;
		case lb_end:
			break;
		case lb_noop:
			counter++;
			break;
		}
	}
	return back;
}

byte_t *seek_to_next_control(byte_t *off, byte_t *end)
{
	return NULL;
}

compile_error_t *handle_if_style_link(byte_t *data, compile_error_t *back)
{
	return NULL;
}
