#include "collections.h"

#include <stdlib.h>
#include <stdarg.h>

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
	const char *dummy = va_arg(ls, const char *);
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