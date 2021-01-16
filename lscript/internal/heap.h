#if !defined(HEAP_H)
#define HEAP_H

#include <stdlib.h>

typedef unsigned long long tag_t;

typedef struct heap_s heap_t;
struct heap_s
{
	tag_t *block;
	tag_t *ptr;
	tag_t *end;
};

typedef heap_t *heap_p;

heap_p create_heap(size_t size);
void free_heap(heap_p heap);

void *halloc(heap_p heap, size_t size);
void hfree(heap_p heap, void *block);

#endif