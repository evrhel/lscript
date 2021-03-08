#include "buffer.h"

#if defined(_WIN32)
#include <Windows.h>
#endif

#include <stdio.h>

static buffer_t *__ensure_capacity(buffer_t *buf, size_t required);

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

#if defined(_DEBUG)

#define YELLOW (FOREGROUND_RED | FOREGROUND_GREEN)
#define RED (FOREGROUND_RED)
#define GREEN (FOREGROUND_GREEN)

#define SAFETY_SIZE 16
#define SAFETY_BYTE 0xaf

typedef struct block_link_s block_link_t;
typedef struct buffer_link_s buffer_link_t;

struct block_link_s
{
	void *block;
	size_t size;
	const char *file;
	int line;
	buffer_link_t *next, *prev;
};

struct buffer_link_s
{
	buffer_t *buf;
	const char *file;
	int line;
	buffer_link_t *next, *prev;
};

static block_link_t *g_blockList = NULL;
static buffer_link_t *g_bufferList = NULL;

static inline void print_console(short color, const char *format, ...)
{
	short old;

	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(hConsole, &info);
	old = info.wAttributes;
	
	SetConsoleTextAttribute(hConsole, color);

	va_list ls;
	va_start(ls, format);
	vprintf(format, ls);
	va_end(ls);

	SetConsoleTextAttribute(hConsole, old);
}

void __begin_debug_d()
{
}

void *__malloc_d(size_t size, const char *file, int line)
{
	print_console(YELLOW, "MALLOC: size %ull at %s.%d\n", size, file, line);
	char *block = (char *)malloc(size + (2 * SAFETY_SIZE));
	if (!block)
	{
		print_console(RED, "MALLOC_ERROR: failed to allocate at %s.%d\n", file, line);
		return NULL;
	}

	if (!g_bufferList)
	{
		g_blockList = (block_link_t *)malloc(sizeof(block_link_t));
		if (!g_blockList)
			return block;
		g_blockList->block = block;
		g_blockList->file = file;
		g_blockList->line = line;
		g_blockList->next = g_blockList->prev = NULL;
	}
	else
	{
		block_link_t *link = (block_link_t *)malloc(sizeof(block_link_t));
		if (!link)
			return block + SAFETY_SIZE;
		link->file = file;
		link->line = line;
		g_blockList->prev = link;
		link->next = g_bufferList;
		link->prev = NULL;
		link->block = block;
		g_blockList = link;
	}

	memset(block, SAFETY_BYTE, SAFETY_SIZE);
	memset(block + SAFETY_SIZE + size, SAFETY_BYTE, SAFETY_SIZE);

	return block + SAFETY_SIZE;
}

void *__calloc_d(size_t count, size_t size, const char *file, int line)
{
	return __malloc_d(count * size, file, line);
}

void *__memcpy_d(void *dst, const void *src, size_t size)
{
	return memcpy(dst, src, size);
}

void __free_d(void *block, const char *file, int line)
{
	block_link_t *curr, *next;
	int found = 0;
	char *cblock = (char *)block;
	if (block == NULL)
		return;

	curr = g_blockList;

	while (curr)
	{
		next = curr->next;

		if (curr->block == block)
		{
			curr->block = NULL;
			found = 1;
			break;
		}

		curr = next;
	}

	if (!found)
	{
		print_console(RED, "FREE_WARNING: Freeing something which was not allocated properly!\n");
		print_console(GREEN, "FREE: %p, at %s.%d (unknown source)\n", (void *)block, file, line);
		free(block);
	}
	else
	{
		print_console(GREEN, "FREE: %p, size %ull at %s.%d (from %s.%d)\n", (void *)block, curr->size, file, line, curr->file, curr->line);
		char *cur = cblock - SAFETY_SIZE;
		while (cur < cblock)
		{
			if (*cur != SAFETY_BYTE)
			{
				print_console(RED, "FREE_WARNING: Detected buffer underflow at %p, size %ull at %s.%d (from %s.%d)\n", (void *)block, curr->size, file, line, curr->file, curr->line);
				return;
			}
			cur++;
		}

		cur = cblock + curr->size;
		char *end = cur + SAFETY_BYTE;
		while (cur < end)
		{
			if (*cur != SAFETY_BYTE)
			{
				print_console(RED, "FREE_WARNING: Detected buffer overflow at %p, size %ull at %s.%d (from %s.%d)\n", (void *)block, curr->size, file, line, curr->file, curr->line);
				return;
			}
			cur++;
		}

		free(cblock - SAFETY_SIZE);
	}
}

buffer_t *__new_buffer_d(size_t size, const char *file, int line)
{
	print_console(YELLOW, "NEW_BUFFER: size %ull at %s.%d\n", size, file, line);
	buffer_t *buf = __new_buffer(size);
	if (!buf)
	{
		print_console(RED, "NEW_BUFFER_ERROR: failed to allocate at %s.%d\n", file, line);
	}
	check(buf);

	if (!g_bufferList)
	{
		g_bufferList = (buffer_link_t *)malloc(sizeof(buffer_link_t));
		if (!g_bufferList)
			return buf;
		g_bufferList->buf = buf;
		g_bufferList->file = file;
		g_bufferList->line = line;
		g_bufferList->next = g_bufferList->prev = NULL;
	}
	else
	{
		buffer_link_t *link = (buffer_link_t *)malloc(sizeof(buffer_link_t));
		if (!link)
			return buf;
		link->file = file;
		link->line = line;
		g_bufferList->prev = link;
		link->next = g_bufferList;
		link->prev = NULL;
		link->buf = buf;
		g_bufferList = link;
	}

	return buf;
}

void __free_buffer_d(buffer_t *buf, const char *file, int line)
{
	buffer_link_t *curr, *next;
	int found = 0;

	curr = g_bufferList;

	while (curr)
	{
		next = curr->next;
		if (curr->buf == buf)
		{
			curr->buf = NULL;
			found = 1;
			break;
		}

		curr = next;
	}

	if (!found)
	{
		print_console(RED, "FREE_BUFFER_WARNING: Freeing something which is not likely to be a buffer!\n");
		print_console(GREEN, "FREE_BUFFER: %p, offset %ull, size %ull at %s.%d (unknown source)\n", (void *)buf, (size_t)(buf->cursor - buf->buf), (size_t)(buf->end - buf->buf), file, line);
		check(buf);
	}
	else
	{
		print_console(GREEN, "FREE_BUFFER: %p, offset %ull, size %ull at %s.%d (from %s.%d)\n", (void *)buf, (size_t)(buf->cursor - buf->buf), (size_t)(buf->end - buf->buf), file, line, curr->file, curr->line);
		check(buf);
	}

	__free_buffer(buf);
}

buffer_t *__put_char_d(buffer_t *buf, char c, const char *file, int line)
{
	return __put_char(buf, c);
}

buffer_t *__put_short_d(buffer_t *buf, short s, const char *file, int line)
{
	return __put_short(buf, s);
}

buffer_t *__put_int_d(buffer_t *buf, int i, const char *file, int line)
{
	return __put_int(buf, i);
}

buffer_t *__put_long_d(buffer_t *buf, long long l, const char *file, int line)
{
	return __put_long(buf, l);
}

buffer_t *__put_float_d(buffer_t *buf, float f, const char *file, int line)
{
	return __put_float(buf, f);
}

buffer_t *__put_double_d(buffer_t *buf, double d, const char *file, int line)
{
	return __put_double(buf, d);
}

buffer_t *__put_mem_d(buffer_t *buf, const void *mem, size_t size, const char *file, int line)
{
	return __put_mem(buf, mem, size);
}

buffer_t *__put_bytes_d(buffer_t *buf, char byte, size_t count, const char *file, int line)
{
	return __put_bytes(buf, byte, count);
}

void __end_debug_d()
{
	size_t count = 0;

	block_link_t *bcurr, *bnext;
	bcurr = g_blockList;
	count = 0;

	while (bcurr)
	{
		bnext = bcurr->next;

		if (bcurr->block != NULL)
		{
			print_console(RED, "END_DEBUG_WARNING: Block %p not freed properly, (from %s.%d).\n", (void *)bcurr->block, bcurr->file, bcurr->line);
			count++;
		}

		bcurr = bnext;
	}

	if (count == 0)
	{
		print_console(GREEN, "END_DEBUG_INFO: All blocks properly freed!\n");
	}
	else
	{
		print_console(RED, "END_DEBUG_WARNING: %ull blocks not freed.\n", count);
	}




	buffer_link_t *curr, *next;
	curr = g_bufferList;
	count = 0;

	while (curr)
	{
		next = curr->next;

		if (curr->buf != NULL)
		{
			print_console(RED, "END_DEBUG_WARNING: Buffer %p not freed properly, (from %s.%d).\n", (void *)curr->buf, curr->file, curr->line);
			count++;
		}

		curr = next;
	}

	if (count == 0)
	{
		print_console(GREEN, "END_DEBUG_INFO: All buffers properly freed!\n");
	}
	else
	{
		print_console(RED, "END_DEBUG_WARNING: %ull buffers not freed.\n", count);
	}
}

#endif

buffer_t *__new_buffer(size_t size)
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

void __free_buffer(buffer_t *buf)
{
	if (buf)
	{
		free(buf->buf);
		free(buf);
	}
}

buffer_t *__put_char(buffer_t *buf, char c)
{
	buf = __ensure_capacity(buf, sizeof(char));
	*buf->cursor = c;
	buf->cursor++;
	CHECK(buf);
	return buf;
}

buffer_t *__put_short(buffer_t *buf, short s)
{
	buf = __ensure_capacity(buf, sizeof(short));
	short *sb = (short *)buf->cursor;
	*sb = s;
	buf->cursor += sizeof(short);
	CHECK(buf);
	return buf;
}

buffer_t *__put_int(buffer_t *buf, int i)
{
	buf = __ensure_capacity(buf, sizeof(int));
	int *ib = (int *)buf->cursor;
	*ib = i;
	buf->cursor += sizeof(int);
	CHECK(buf);
	return buf;
}

buffer_t *__put_long(buffer_t *buf, long long l)
{
	buf = __ensure_capacity(buf, sizeof(long long));
	long long *lb = (long long *)buf->cursor;
	*lb = l;
	buf->cursor += sizeof(long long);
	CHECK(buf);
	return buf;
}

buffer_t *__put_float(buffer_t *buf, float f)
{
	buf = __ensure_capacity(buf, sizeof(float));
	float *fb = (float *)buf->cursor;
	*fb = f;
	buf->cursor += sizeof(float);
	CHECK(buf);
	return buf;
}

buffer_t *__put_double(buffer_t *buf, double d)
{
	buf = __ensure_capacity(buf, sizeof(double));
	double *db = (double *)buf->cursor;
	*db = d;
	buf->cursor += sizeof(double);
	CHECK(buf);
	return buf;
}

buffer_t *__put_mem(buffer_t *buf, const void *mem, size_t size)
{
	buf = __ensure_capacity(buf, size);
	memcpy(buf->cursor, mem, size);
	buf->cursor += size;
	CHECK(buf);
	return buf;
}

buffer_t *__ensure_capacity(buffer_t *buf, size_t required)
{
	size_t avaliable = (size_t)(buf->end - buf->cursor);
	if (avaliable < required)
	{
		size_t totalSize = (size_t)(buf->end - buf->buf);
		size_t cursorOff = (size_t)(buf->cursor - buf->buf);
		size_t newSize = totalSize * 2;
		char *nbuf = (char *)realloc(buf->buf, newSize);
			//malloc(newSize);
		if (!nbuf)
			return NULL;
		//memcpy(nbuf, buf->buf, cursorOff);
		//free(buf->buf);
		buf->buf = nbuf;
		buf->end = buf->buf + newSize;
		buf->cursor = buf->buf + cursorOff;
		return __ensure_capacity(buf, required);
	}
	return buf;
}

