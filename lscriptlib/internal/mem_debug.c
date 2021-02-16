#include "mem_debug.h"
#if defined(_DEBUG) && defined(ENABLE_DEBUG)

#include "collection.h"

#include <stdio.h>
#include <stdarg.h>

#include <Windows.h>

#define PADDING_SIZE sizeof(size_t)
#define INFO_TO_PAYLOAD(info) (((char*)info)+PADDING_SIZE+sizeof(block_info_t))
#define PAYLOAD_TO_INFO(payl) (((char*)payl)-PADDING_SIZE-sizeof(block_info_t))

enum
{
	MESSAGE_INFO = 0,
	MESSAGE_WARNING,
	MESSAGE_ERROR
};

static map_t *g_blockMap = NULL;
static map_t *g_allocLocs = NULL;
static FILE *g_outputFile = NULL;
static int lock = 0;

typedef struct block_info_s block_info_t;
struct block_info_s
{
	size_t payloadSize;
};

static void log_event(int messageType, const char *message, ...);
static LONG WINAPI exception_filter(EXCEPTION_POINTERS *);

int __dbegin_debug()
{
	fopen_s(&g_outputFile, "memdebug.txt", "w");
	if (!g_outputFile)
		return 0;

	SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)exception_filter);

	g_blockMap = map_create(16, NULL, NULL, NULL, NULL, NULL);
	g_allocLocs = map_create(16, NULL, NULL, NULL, string_copy_func, NULL);

	log_event(MESSAGE_INFO, "Memory debugger initialized");

	return 1;
}

void *__dmalloc(size_t size, const char *filename, int line)
{
	lock = 1;
	size_t totalSize = size + sizeof(block_info_t) + (2 * PADDING_SIZE);

	log_event(MESSAGE_INFO, "Allocating block size %d at %s.%d", (int)size, filename, line);

	block_info_t *block = (block_info_t *)malloc(totalSize);
	if (!block)
	{
		log_event(MESSAGE_WARNING, "malloc returned NULL on allocation size %d", (int)size);
		lock = 0;
		return NULL;
	}
	block->payloadSize = size;

	size_t *frontPadding = (size_t *)(block + 1);
	size_t *backPadding = frontPadding + (size / PADDING_SIZE);
	*frontPadding = 0xdeadbeefdeadbeef;
	*backPadding = 0xdeadbeefdeadbeef;

	void *returnAddr = frontPadding + 1;
	if (g_blockMap && g_allocLocs)
	{
		if (!lock)
			map_insert(g_blockMap, returnAddr, block);


		char buf[100];
		sprintf_s(buf, sizeof(buf), "%s.%d", filename, line);

		if (!lock)
			map_insert(g_allocLocs, returnAddr, buf);

	}

	lock = 0;
	return returnAddr;
}

void *__dcalloc(size_t count, size_t size, const char *filename, int line)
{
	void *block = __dmalloc(count * size, filename, line);
	if (!block)
		return NULL;
	memset(block, 0, count * size);
	return block;
}

void __dfree(void *block, const char *filename, int line)
{
	if (!block)
		return;
	lock = 1;
	if (g_blockMap && g_allocLocs)
	{
		block_info_t *info = (block_info_t *)map_at(g_blockMap, block);
		if (!info)
		{
			log_event(MESSAGE_WARNING, "Invalid block 0x%p attempting to be freed at %s.%d", block, filename, line);
		}
		else
		{
			log_event(MESSAGE_INFO, "Freeing block 0x%p at %s.%d", block, filename, line);

			if (!lock)
			{
				map_remove(g_blockMap, block);
				map_remove(g_allocLocs, block);
			}

			size_t *header = (size_t *)info + 1;
			size_t *footer = (size_t *)header + (info->payloadSize / PADDING_SIZE);

			if (*header != 0xdeadbeefdeadbeef)
				log_event(MESSAGE_WARNING, "Block 0x%p front padding corrupted", block);
			if (*footer != 0xdeadbeefdeadbeef)
				log_event(MESSAGE_WARNING, "Block 0x%p back padding corrupted", block);
		}
		free(info);
	}
	else
	{
		log_event(MESSAGE_WARNING, "Freeing block 0x%p with no maps", block);
		free(PAYLOAD_TO_INFO(block));
	}
	lock = 0;
}

void *__dmemset(void *dst, int val, size_t size, const char *filename, int line)
{
	log_event(MESSAGE_INFO, "memset 0x%p to %d size %d at %s.%d", dst, val, (int)size, filename, line);
	return memset(dst, val, size);
}

void *__dmemcpy(void *dst, void *src, size_t size, const char *filename, int line)
{
	log_event(MESSAGE_INFO, "memcpy 0x%p to 0x%p size %d at %s.%d", src, dst, (int)size, filename, line);
	return memcpy(dst, src, size);
}

int __dend_debug()
{
	char buf[100];

	if (g_blockMap && g_allocLocs)
	{

		map_iterator_t *it = map_create_iterator(g_blockMap);
		while (it->node)
		{
			void *block = it->key;
			block_info_t *info = (block_info_t *)it->value;

			map_node_t *node = map_find(g_allocLocs, block);

			log_event(MESSAGE_WARNING, "0x%p (size %d) was never freed (allocated at %s)", it->key, (int)info->payloadSize, (const char *)node->value);

			free(node->value);
			node->value = NULL;

			size_t *header = (size_t *)info + 1;
			size_t *footer = (size_t *)header + (info->payloadSize / PADDING_SIZE);

			if (*header != 0xdeadbeefdeadbeef)
				log_event(MESSAGE_WARNING, "Block 0x%p front padding corrupted", block);
			if (*footer != 0xdeadbeefdeadbeef)
				log_event(MESSAGE_WARNING, "Block 0x%p back padding corrupted", block);

			it = map_iterator_next(it);
		}
		map_iterator_free(it);

		map_free(g_blockMap, 0);
		g_blockMap = NULL;

		map_free(g_allocLocs, 0);
		g_allocLocs = NULL;
	}

	log_event(MESSAGE_INFO, "Memory debugger ending");

	fclose(g_outputFile);
	g_outputFile = NULL;
}

void log_event(int messageType, const char *message, ...)
{
	va_list ls;
	va_start(ls, message);

	const char *typeString;
	switch (messageType)
	{
	case MESSAGE_INFO:
		typeString = "[INFO]";
		break;
	case MESSAGE_WARNING:
		typeString = "[WARN]";
		break;
	case MESSAGE_ERROR:
		typeString = "[ERROR]";
		break;
	default:
		typeString = "[UNKN]";
		break;
	}
	fprintf_s(g_outputFile, "%s\t: ", typeString);
	vfprintf_s(g_outputFile, message, ls);
	fprintf_s(g_outputFile, "\n");
	if (messageType >= MESSAGE_WARNING)
	{
		fprintf_s(stderr, "%s\t: ", typeString);
		vfprintf_s(stderr, message, ls);
		fprintf_s(stderr, "\n");
	}

	va_end(ls);
}

LONG WINAPI exception_filter(EXCEPTION_POINTERS *p)
{
	if (g_outputFile)
		fclose(g_outputFile);
	fprintf_s(stderr, "FATAL ERROR\n");
	return EXCEPTION_EXECUTE_HANDLER;
}


#endif
