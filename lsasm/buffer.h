#if !defined(BUFFER_H)
#define BUFFER_H

#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <internal/types.h>

typedef struct buffer_s buffer_t;
struct buffer_s
{
	char *buf;
	char *cursor;
	char *end;
};

#if defined(_DEBUG)

void __begin_debug_d();

void *__malloc_d(size_t size, const char *file, int line);
void *__calloc_d(size_t count, size_t size, const char *file, int line);
void *__memcpy_d(void *dst, const void *src, size_t size, const char *file, int line);
void __free_d(void *block, const char *file, int line);

buffer_t *__new_buffer_d(size_t size, const char *file, int line);
void __free_buffer_d(buffer_t *buf, const char *file, int line);

buffer_t *__put_char_d(buffer_t *buf, char c, const char *file, int line);
buffer_t *__put_short_d(buffer_t *buf, short s, const char *file, int line);
buffer_t *__put_int_d(buffer_t *buf, int i, const char *file, int line);
buffer_t *__put_long_d(buffer_t *buf, long long l, const char *file, int line);
buffer_t *__put_float_d(buffer_t *buf, float f, const char *file, int line);
buffer_t *__put_double_d(buffer_t *buf, double d, const char *file, int line);

buffer_t *__put_mem_d(buffer_t *buf, const void *mem, size_t size, const char *file, int line);
buffer_t *__put_bytes_d(buffer_t *buf, char byte, size_t count, const char *file, int line);

void __end_debug_d();

#define BEGIN_DEBUG() __begin_debug_d()

#define MALLOC(size) __malloc_d(size, strrchr(__FILE__, '\\') + 1, __LINE__)
#define CALLOC(count, size) __calloc_d(count, size, strrchr(__FILE__, '\\') + 1, __LINE__)
#define MEMCPY(dst, src, size) __memcpy_d(dst, src, size, strrchr(__FILE__, '\\') + 1, __LINE__)
#define FREE(block) __free_d(block, strrchr(__FILE__, '\\') + 1, __LINE__)

#define NEW_BUFFER(size) __new_buffer_d(size, strrchr(__FILE__, '\\') + 1, __LINE__)
#define FREE_BUFFER(buffer) __free_buffer_d(buffer, strrchr(__FILE__, '\\') + 1, __LINE__)
#define PUT_CHAR(buffer, c) __put_char_d(buffer, c, strrchr(__FILE__, '\\') + 1, __LINE__)
#define PUT_UCHAR(buffer, uc) PUT_CHAR(buffer, (char)(uc))
#define PUT_BYTE(buffer, b) PUT_CHAR(buffer, (char)(b))
#define PUT_SHORT(buffer, s) __put_short_d(buffer, s, strrchr(__FILE__, '\\') + 1, __LINE__)
#define PUT_USHORT(buffer, us) PUT_SHORT(buffer, (short)(us))
#define PUT_INT(buffer, i) __put_int_d(buffer, i, strrchr(__FILE__, '\\') + 1, __LINE__)
#define PUT_UINT(buffer, ui) PUT_INT(buffer, (int)(ui))
#define PUT_LONG(buffer, l) __put_long_d(buffer, l, strrchr(__FILE__, '\\') + 1, __LINE__)
#define PUT_ULONG(buffer, ul) PUT_LONG(buffer, (long long)(ul))
#define PUT_FLOAT(buffer, f) __put_float_d(buffer, f, strrchr(__FILE__, '\\') + 1, __LINE__)
#define PUT_DOUBLE(buffer, d) __put_double_d(buffer, d, strrchr(__FILE__, '\\') + 1, __LINE__)
#define PUT_MEM(buffer, mem, size) __put_mem_d(buffer, mem, size, strrchr(__FILE__, '\\') + 1, __LINE__)
#define PUT_BUF(buffer, other) PUT_MEM(buffer, (other)->buf, (size_t)((other)->cursor - (other)->buf))
#define PUT_BYTES(buffer, byte, count) __put_bytes_d(buffer, byte, count, strrchr(__FILE__, '\\') + 1, __LINE__)
#define PUT_STRING(buffer, str) PUT_MEM(buffer, str, strlen(str) + 1)

#define END_DEBUG() __end_debug_d()

#else

#define BEGIN_DEBUG()

#define MALLOC(size) malloc(size)
#define CALLOC(count, size) calloc(count, size)
#define MEMCPY(dst, src, size) memcpy(dst, src, size)
#define FREE(block) free(block)

#define NEW_BUFFER(size) __new_buffer(size)
#define FREE_BUFFER(buffer) __free_buffer(buffer)
#define PUT_CHAR(buffer, c) __put_char(buffer, c)
#define PUT_UCHAR(buffer, uc) PUT_CHAR(buffer, (char)(uc))
#define PUT_BYTE(buffer, b) PUT_CHAR(buffer, (char)(b))
#define PUT_SHORT(buffer, s) __put_short(buffer, s)
#define PUT_USHORT(buffer, us) PUT_SHORT(buffer, (short)(us))
#define PUT_INT(buffer, i) __put_int(buffer, i)
#define PUT_UINT(buffer, ui) PUT_INT(buffer, (int)(ui))
#define PUT_LONG(buffer, l) __put_long(buffer, l)
#define PUT_ULONG(buffer, ul) PUT_LONG(buffer, (long long)(ul))
#define PUT_FLOAT(buffer, f) __put_float(buffer, f)
#define PUT_DOUBLE(buffer, d) __put_double(buffer, d)
#define PUT_MEM(buffer, mem, size) __put_mem(buffer, mem, size)
#define PUT_BUF(buffer, other) PUT_MEM(buffer, (other)->buf, (size_t)((other)->cursor - (other)->buf))
#define PUT_BYTES(buffer, byte, count) __put_bytes(buffer, byte, count)
#define PUT_STRING(buffer, str) PUT_MEM(buffer, str, strlen(str) + 1)

#define END_DEBUG()

#endif

buffer_t *__new_buffer(size_t size);
void __free_buffer(buffer_t *buf);

buffer_t *__put_char(buffer_t *buf, char c);
buffer_t *__put_short(buffer_t *buf, short s);
buffer_t *__put_int(buffer_t *buf, int i);
buffer_t *__put_long(buffer_t *buf, long long l);
buffer_t *__put_float(buffer_t *buf, float f);
buffer_t *__put_double(buffer_t *buf, double d);

buffer_t *__put_mem(buffer_t *buf, const void *mem, size_t size);

inline buffer_t *__put_bytes(buffer_t *buf, char byte, size_t count)
{
	char *tmp = (char *)malloc(count);
	if (!tmp)
		return NULL;
	memset(tmp, byte, count);
	buf = __put_mem(buf, tmp, count);
	free(tmp);
	return buf;
}


#endif