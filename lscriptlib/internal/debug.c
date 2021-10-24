#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

debug_t *load_debug(const char *path)
{
	debug_t *obj = (debug_t *)malloc(sizeof(debug_t));
	if (!obj)
		return NULL;

	FILE *in;
	fopen_s(&in, path, "rb");

	if (!in)
	{
		free(obj);
		return NULL;
	}

	fseek(in, 0, SEEK_END);
	long len = ftell(in);
	fseek(in, 0, SEEK_SET);

	obj->buf = malloc(len);
	if (!obj->buf)
	{
		fclose(in);
		free(obj);
		return NULL;
	}

	fread_s(obj->buf, len, sizeof(char), len, in);

	fclose(in);

	obj->version = *((unsigned int *)obj->buf);
	obj->srcFile = (char *)obj->buf + sizeof(unsigned int);
	obj->first = (debug_elem_t *)(obj->srcFile + strlen(obj->srcFile) + 1);
	obj->last = (debug_elem_t *)((char *)obj->buf + len - sizeof(debug_elem_t));

	return obj;
}

debug_elem_t *find_debug_elem(debug_t *debug, unsigned int off)
{
	debug_elem_t *curr = debug->first;
	while (curr < debug->last)
	{
		if (curr->binOff >= off)
			return curr;
		curr++;
	}
	return curr > debug->last ? NULL : curr;
}

void free_debug(debug_t *debug)
{
	if (debug)
	{
		if (debug->buf)
		{
			free(debug->buf);
			debug->buf = NULL;
		}

		free(debug);
	}
}
