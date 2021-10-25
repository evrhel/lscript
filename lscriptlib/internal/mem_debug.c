#include "mem_debug.h"

#include <Windows.h>
#include <assert.h>

#if defined(_DEBUG) && defined(ENABLE_DEBUG) || defined(FORCE_DEBUG)

#include "collection.h"

#include <stdio.h>
#include <stdarg.h>

#define PADDING_COUNT 1
#define PADDING_SIZE sizeof(size_t)
#define INFO_TO_PAYLOAD(info) (((char*)info)+PADDING_SIZE+sizeof(block_info_t))
#define PAYLOAD_TO_INFO(payl) (((char*)payl)-PADDING_SIZE-sizeof(block_info_t))

#define lock() (ignore=1)
#define unlock() (ignore=0)
#define locked() (ignore)

enum
{
	MESSAGE_INFO = 0,
	MESSAGE_WARNING,
	MESSAGE_ERROR
};

static map_t *g_blockMap = NULL;
static FILE *g_outputFile = NULL;
static int ignore = 0;

typedef struct block_info_s block_info_t;
struct block_info_s
{
	size_t payloadSize;

	const char *allocFile;
	int allocLine;

	int freeLine;
	const char *freeFile;

	int isFreed;

	DWORD padding;
};

static void log_event(int messageType, const char *message, ...);
static LONG WINAPI exception_filter(EXCEPTION_POINTERS *);
static void *find_src_block(void *intermediate, block_info_t **info);

inline static block_info_t *info_from_addr(void *addr)
{
	return (block_info_t *)(((char *)addr) - sizeof(block_info_t) - (8 * PADDING_SIZE * PADDING_COUNT));
}

const char *__dsafe_strrchr(const char *str, int ch)
{
	const char *last = NULL;
	const char *cursor = str;
	while (*cursor)
	{
		if (*cursor == ch) last = cursor;
		cursor++;
	}
	return last;
}

int __dbegin_debug()
{
	assert(!g_blockMap && !g_outputFile);

	fopen_s(&g_outputFile, "memdebug.txt", "w");
	if (!g_outputFile)
		return 0;

	SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)exception_filter);

	lock();

	g_blockMap = map_create(5, NULL, NULL, NULL, NULL, NULL);

	unlock();

	log_event(MESSAGE_INFO, "Memory debugger initialized");

	return 1;
}

void *__dmalloc(size_t size, const char *filename, int line)
{
	if (locked()) return malloc(size);

	assert(g_blockMap && g_outputFile);

	size_t totalSize = size + sizeof(block_info_t) + ((2 * PADDING_SIZE) * PADDING_COUNT);

	block_info_t *block = (block_info_t *)malloc(totalSize);
	if (!block)
	{
		log_event(MESSAGE_WARNING, "alloc(%llu) returned NULL", size);
		return NULL;
	}

	block->payloadSize = size;
	block->allocFile = filename;
	block->allocLine = line;
	block->freeFile = "null";
	block->freeLine = 0;
	block->isFreed = 0;

	size_t *frontPadding = (size_t *)(block + 1);
	size_t *returnAddr = frontPadding + PADDING_COUNT;

	log_event(MESSAGE_INFO, "0x%p: alloc(%llu) at %s.%d", returnAddr, size, filename, line);

	size_t *backPadding = (size_t *)((char *)returnAddr + size);
	if (PADDING_COUNT)
	{
		*frontPadding = 0xdeadbeefdeadbeef;
		*backPadding = 0xdeadbeefdeadbeef;
	}

	memset(returnAddr, 0xaa, size);

	lock();

	map_insert(g_blockMap, returnAddr, block);

	unlock();

	return returnAddr;
}

void *__dcalloc(size_t count, size_t size, const char *filename, int line)
{
	if (locked()) return calloc(count, size);

	assert(g_blockMap && g_outputFile);

	void *block = __dmalloc(count * size, filename, line);
	if (!block)
		return NULL;
	memset(block, 0, count * size);
	return block;
}

void __dfree(void *block, const char *filename, int line)
{
	if (locked()) return free(block);

	assert(g_blockMap && g_outputFile);

	if (!block)
		return;

	block_info_t *info = (block_info_t *)map_at(g_blockMap, block);
	if (!info)
	{
		log_event(MESSAGE_WARNING, "free(0x%p) at %s.%d (from: UNKNOWN)", block, filename, line);
		free(info);
	}
	else
	{
		log_event(MESSAGE_INFO, "free(0x%p size %d) at %s.%d (from: %s.%d)", block, info->payloadSize, filename, line, info->allocFile, info->allocLine);
		if (info->isFreed)
		{
			log_event(MESSAGE_ERROR, "free(0x%p size %d) double free at %s.%d (from: %s.%d)", block, info->payloadSize, filename, line, info->allocFile, info->allocLine);
			return;
		}
		
		/*lock();

		map_remove(g_blockMap, block);

		unlock();*/

		size_t *header = (size_t *)(info + 1);
		size_t *payload = header + PADDING_COUNT;
		size_t *footer = (size_t *)((char *)payload + info->payloadSize);

		memset(payload, 0x69, info->payloadSize);

		if (PADDING_COUNT)
		{
			if (*header != 0xdeadbeefdeadbeef)
				log_event(MESSAGE_WARNING, "free(0x%p) front padding corrupted at %s.%d (from: %s.%d)", block, filename, line, info->allocFile, info->allocLine);
			if (*footer != 0xdeadbeefdeadbeef)
				log_event(MESSAGE_WARNING, "free(0x%p) back padding corrupted at %s.%d (from: %s.%d)", block, filename, line, info->allocFile, info->allocLine);
		}

		info->isFreed = 1;
		info->freeFile = filename;
		info->freeLine = line;
	}
}

void *__dmemset(void *dst, int val, size_t size, const char *filename, int line)
{
	assert(g_blockMap && g_outputFile);

	block_info_t *dstInfo;
	void *dstblock;

	lock();

	if (!(dstblock = find_src_block(dst, &dstInfo)))
	{
		unlock();

		log_event(MESSAGE_WARNING, "memset(0x%p to %d size %llu) at %s.%d (from: UNKNOWN)", dst, val, size, filename, line);
		return memset(dst, val, size);
	}

	unlock();

	log_event(MESSAGE_INFO, "memset 0x%p (from: %s.%d) to %d size %llu at %s.%d", dst, dstInfo->allocFile, dstInfo->allocLine, val, size, filename, line);

	size_t *header = (size_t *)(dstInfo + 1);
	size_t *footer = (size_t *)((char *)(header + PADDING_COUNT) + dstInfo->payloadSize);

	if (PADDING_COUNT)
	{
		if (*header != 0xdeadbeefdeadbeef)
			log_event(MESSAGE_WARNING, "memcpy: Block 0x%p front padding corrupted (from: %s.%d)", dstblock, dstInfo->allocFile, dstInfo->allocLine);
		if (*footer != 0xdeadbeefdeadbeef)
			log_event(MESSAGE_WARNING, "memcpy: Block 0x%p back padding corrupted (from: %s.%d)", dstblock, dstInfo->allocFile, dstInfo->allocLine);
	}

	return memset(dst, val, size);
}

void *__dmemcpy(void *dst, void *src, size_t size, const char *filename, int line)
{
	assert(g_blockMap && g_outputFile);

	block_info_t *dstInfo;
	void *dstblock;
	if (!(dstblock = find_src_block(dst, &dstInfo)))
	{
		log_event(MESSAGE_WARNING, "memcpy(0x%p to 0x%p (from: UNKNOWN) size %llu) at %s.%d", src, dst, size, filename, line);
		return memcpy(dst, src, size);
	}

	log_event(MESSAGE_INFO, "memcpy(0x%p to 0x%p (from: %s.%d) size %d) at %s.%d", src,
		dst, dstInfo->allocFile, dstInfo->allocLine, (int)size, filename, line);

	size_t *header = (size_t *)(dstInfo + 1);
	size_t *footer = (size_t *)((char *)(header + PADDING_COUNT) + dstInfo->payloadSize);

	if (PADDING_COUNT)
	{
		if (*header != 0xdeadbeefdeadbeef)
			log_event(MESSAGE_WARNING, "memcpy: Block 0x%p front padding corrupted (from: %s.%d)", dstblock, dstInfo->allocFile, dstInfo->allocLine);
		if (*footer != 0xdeadbeefdeadbeef)
			log_event(MESSAGE_WARNING, "memcpy: Block 0x%p back padding corrupted (from: %s.%d)", dstblock, dstInfo->allocFile, dstInfo->allocLine);
	}

	return memcpy(dst, src, size);
}

int __dend_debug()
{
	assert(g_blockMap && g_outputFile);

	char buf[100];

	lock();
	
	map_iterator_t *it = map_create_iterator(g_blockMap);
	while (it->node)
	{
		void *block = it->key;
		block_info_t *info = (block_info_t *)it->value;

		size_t *header = (size_t *)(info + 1);
		char *payload = (char *)(header + PADDING_COUNT);
		size_t *footer = (size_t *)(payload + info->payloadSize);

		if (info->isFreed)
		{
			for (size_t i = 0; i < info->payloadSize; i++)
			{
				if (payload[i] != 0x69)
				{
					log_event(MESSAGE_WARNING, "end_debug(): Block 0x%p modified after free (from: %s.%d, freed: %s.%d)", block, info->allocFile, info->allocLine, info->freeFile, info->freeLine);
					return;
				}
			}
		}
		else
		{
			log_event(MESSAGE_WARNING, "end_debug(): 0x%p (size %llu) was never freed (from: %s.%d)", it->key, info->payloadSize, info->allocFile, info->allocLine);
		}

		if (PADDING_COUNT)
		{
			if (*header != 0xdeadbeefdeadbeef)
				log_event(MESSAGE_WARNING, "end_debug(): Block 0x%p front padding corrupted (from: %s.%d, freed: %s.%d)", block, info->allocFile, info->allocLine, info->freeFile, info->freeLine);
			if (*footer != 0xdeadbeefdeadbeef)
				log_event(MESSAGE_WARNING, "end_debug(): Block 0x%p back padding corrupted (from: %s.%d, freed: %s.%d)", block, info->allocFile, info->allocLine, info->freeFile, info->freeLine);
		}

		it = map_iterator_next(it);
	}
	map_iterator_free(it);

	map_free(g_blockMap, 0);
	g_blockMap = NULL;

	unlock();

	log_event(MESSAGE_INFO, "Memory debugger ending");

	fclose(g_outputFile);
	g_outputFile = NULL;
}

void __check_corruption()
{
	map_iterator_t *it = map_create_iterator(g_blockMap);
	while (it->node)
	{
		void *block = it->key;
		block_info_t *info = (block_info_t *)it->value;

		size_t *header = (size_t *)(info + 1);
		char *payload = (char *)(header + PADDING_COUNT);
		size_t *footer = (size_t *)(payload + info->payloadSize);

		if (info->isFreed)
		{
			for (size_t i = 0; i < info->payloadSize; i++)
			{
				if (payload[i] != 0x69)
				{
					log_event(MESSAGE_WARNING, "end_debug(): Block 0x%p modified after free (from: %s.%d freed: %s.%d)", block, info->allocFile, info->allocLine, info->freeFile, info->freeLine);
					return;
				}
			}
		}

		if (PADDING_COUNT)
		{
			if (*header != 0xdeadbeefdeadbeef)
				log_event(MESSAGE_WARNING, "end_debug(): Block 0x%p front padding corrupted (from: %s.%d freed: %s.%d)", block, info->allocFile, info->allocLine, info->freeFile, info->freeLine);
			if (*footer != 0xdeadbeefdeadbeef)
				log_event(MESSAGE_WARNING, "end_debug(): Block 0x%p back padding corrupted (from: %s.%d freed: %s.%d)", block, info->allocFile, info->allocLine, info->freeFile, info->freeLine);
		}

		it = map_iterator_next(it);
	}
	map_iterator_free(it);
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
	//fprintf_s(g_outputFile, "%s\t: ", typeString);
	//vfprintf_s(g_outputFile, message, ls);
	////fprintf_s(g_outputFile, "\n");
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

void *find_src_block(void *intermediate, block_info_t **info)
{
	int waslocked = 1;

	map_iterator_t *mit = map_create_iterator(g_blockMap);
	while (mit->node)
	{
		block_info_t *infoPtr = *info = (block_info_t *)mit->value;
		char *startPayload = (char *)((size_t *)(infoPtr + 1) + 1);
		char *endPayload = startPayload + infoPtr->payloadSize;
		char *interm = (char *)intermediate;

		if (interm >= startPayload && interm <= endPayload)
			return startPayload;
		
		mit = map_iterator_next(mit);
	}
	map_iterator_free(mit);

	return NULL;
}


#endif

void __check_native_corruption()
{
	BOOL bResult = HeapValidate(GetProcessHeap(), 0, NULL);
	if (!bResult)
	{
		MessageBoxA(NULL, "Heap corruption detected!", "Error", MB_ICONERROR);
		RaiseException(EXCEPTION_BREAKPOINT, 0, 0, NULL);
	}
}
