#if !defined(MEM_DEBUG_H)
#define MEM_DEBUG_H

#include <stdlib.h>
#include <memory.h>

//#define FORCE_DEBUG

#if defined(_DEBUG) && defined(ENABLE_DEBUG) || defined(FORCE_DEBUG)
#define BEGIN_DEBUG() __dbegin_debug()
#define MALLOC(size) __dmalloc(size, __dsafe_strrchr(__FILE__, '\\') + 1, __LINE__)
#define CALLOC(count, size) __dcalloc(count, size, __dsafe_strrchr(__FILE__, '\\') + 1, __LINE__)
#define FREE(block) __dfree(block, __dsafe_strrchr(__FILE__, '\\') + 1, __LINE__)
#define MEMSET(dst, val, size) __dmemset(dst, val, size, __dsafe_strrchr(__FILE__, '\\') + 1, __LINE__)
#define MEMCPY(dst, src, size) __dmemcpy(dst, src, size, __dsafe_strrchr(__FILE__, '\\') + 1, __LINE__)
#define END_DEBUG() __dend_debug()

const char *__dsafe_strrchr(const char *str, int ch);

int __dbegin_debug();
void *__dmalloc(size_t size, const char *filename, int line);
void *__dcalloc(size_t count, size_t size, const char *filename, int line);
void __dfree(void *block, const char *filename, int line);
void *__dmemset(void *dst, int val, size_t size, const char *filename, int line);
void *__dmemcpy(void *dst, void *src, size_t size, const char *filename, int line);
int __dend_debug();
void __check_corruption();

#else
#define BEGIN_DEBUG()
#define MALLOC(size) malloc(size)
#define CALLOC(count, size) calloc((count), (size))
#define FREE(block) free((void *)(block))
#define MEMSET(dst, val, size) memset((dst), (val), (size))
#define MEMCPY(dst, src, size) memcpy((dst), (src), (size))
#define END_DEBUG()
#endif

void __check_native_corruption();

#endif