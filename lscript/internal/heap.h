#if !defined(HEAP_H)
#define HEAP_H

#include <stdlib.h>

typedef void *heap_t;

heap_t create_heap(size_t size);
void free_heap(heap_t heap);

void *halloc(heap_t heap, size_t size);
void hfree(heap_t heap, void *block);

#endif