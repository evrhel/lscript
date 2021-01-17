#include "buffer.h"

#if defined(_WIN32)
#include <Windows.h>
#endif

static buffer_t *ensure_capacity(buffer_t *buf, size_t required);

#if defined(_DEBUG)
#define CHECK(buffer) check((buffer))
#else
#define CHECK(buffer)
#endif

#if defined(_DEBUG)
static inline void check(buffer_t *buf)
{
	if (buf->cursor > buf->end)
	{
#if defined(_WIN32)
		MessageBoxA(NULL, "Buffer cursor is past the end", "Buffer Corruption!", MB_ICONERROR | MB_OK);
		RaiseException(EXCEPTION_BREAKPOINT, 0, 0, NULL);
#endif
	}
	else if (buf->cursor < buf->buf)
	{
#if defined(_WIN32)
		MessageBoxA(NULL, "Buffer cursor is before the start", "Buffer Corruption!", MB_ICONERROR | MB_OK);
		RaiseException(EXCEPTION_BREAKPOINT, 0, 0, NULL);
#endif
	}
}
#endif

buffer_t *new_buffer(size_t size)
{
	buffer_t *buf = (buffer_t *)malloc(sizeof(buffer_t));
	if (!buf)
		return NULL;
	buf->buf = (char *)malloc(size);
	if (!buf->buf)
	{
		free(buf);
		return NULL;
	}
	buf->cursor = buf->buf;
	buf->end = buf->buf + size;
	return buf;
}

void free_buffer(buffer_t *buf)
{
	if (buf)
	{
		free(buf->buf);
		free(buf);
	}
}

buffer_t *put_char(buffer_t *buf, char c)
{
	buf = ensure_capacity(buf, sizeof(char));
	*buf->cursor = c;
	buf->cursor++;
	CHECK(buf);
	return buf;
}

buffer_t *put_short(buffer_t *buf, short s)
{
	buf = ensure_capacity(buf, sizeof(short));
	short *sb = (short *)buf->cursor;
	*sb = s;
	buf->cursor += sizeof(short);
	CHECK(buf);
	return buf;
}

buffer_t *put_int(buffer_t *buf, int i)
{
	buf = ensure_capacity(buf, sizeof(int));
	int *ib = (int *)buf->cursor;
	*ib = i;
	buf->cursor += sizeof(int);
	CHECK(buf);
	return buf;
}

buffer_t *put_long(buffer_t *buf, long long l)
{
	buf = ensure_capacity(buf, sizeof(long long));
	long long *lb = (long long *)buf->cursor;
	*lb = l;
	buf->cursor += sizeof(long long);
	CHECK(buf);
	return buf;
}

buffer_t *put_float(buffer_t *buf, float f)
{
	buf = ensure_capacity(buf, sizeof(float));
	float *fb = (float *)buf->cursor;
	*fb = f;
	buf->cursor += sizeof(float);
	CHECK(buf);
	return buf;
}

buffer_t *put_double(buffer_t *buf, double d)
{
	buf = ensure_capacity(buf, sizeof(double));
	double *db = (double *)buf->cursor;
	*db = d;
	buf->cursor += sizeof(double);
	CHECK(buf);
	return buf;
}

buffer_t *put_mem(buffer_t *buf, const void *mem, size_t size)
{
	buf = ensure_capacity(buf, size);
	memcpy(buf->cursor, mem, size);
	buf->cursor += size;
	CHECK(buf);
	return buf;
}

buffer_t *ensure_capacity(buffer_t *buf, size_t required)
{
	size_t avaliable = (size_t)(buf->end - buf->cursor);
	if (avaliable < required)
	{
		size_t totalSize = (size_t)(buf->end - buf->buf);
		size_t cursorOff = (size_t)(buf->cursor - buf->buf);
		size_t newSize = totalSize * 2;
		char *nbuf = (char *)malloc(newSize);
		if (!nbuf)
			return NULL;
		memcpy(nbuf, buf->buf, cursorOff);
		free(buf->buf);
		buf->buf = nbuf;
		buf->end = buf->buf + newSize;
		buf->cursor = buf->buf + cursorOff;
		return ensure_capacity(buf, required);
	}
	return buf;
}
