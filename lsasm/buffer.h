#if !defined(BUFFER_H)
#define BUFFER_H

#include <stdlib.h>
#include <memory.h>
#include <string.h>

typedef struct buffer_s buffer_t;
struct buffer_s
{
	char *buf;
	char *cursor;
	char *end;
};

buffer_t *new_buffer(size_t size);
void free_buffer(buffer_t *buf);

buffer_t *put_char(buffer_t *buf, char c);
buffer_t *put_short(buffer_t *buf, short s);
buffer_t *put_int(buffer_t *buf, int i);
buffer_t *put_long(buffer_t *buf, long long l);
buffer_t *put_float(buffer_t *buf, float f);
buffer_t *put_double(buffer_t *buf, double d);

buffer_t *put_mem(buffer_t *buf, const void *mem, size_t size);

inline buffer_t *put_uchar(buffer_t *buf, unsigned char c)
{
	return put_char(buf, (char)c);
}

inline buffer_t *put_ushort(buffer_t *buf, unsigned short s)
{
	return put_short(buf, (short)s);
}

inline buffer_t *put_uint(buffer_t *buf, unsigned int i)
{
	return put_int(buf, (int)i);
}

inline buffer_t *put_ulong(buffer_t *buf, unsigned long long l)
{
	return put_long(buf, (long long)l);
}

inline buffer_t *put_buf(buffer_t *buf, const buffer_t *other)
{
	return put_mem(buf, other->buf, (size_t)(other->cursor - other->buf));
}

inline buffer_t *put_string(buffer_t *buf, const char *str)
{
	return put_mem(buf, str, strlen(str) + 1);
}

#endif