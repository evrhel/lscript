#if !defined(HEAP_H)
#define HEAP_H

#include <stdlib.h>

typedef unsigned long long tag_t;

typedef struct heap_s heap_t;
struct heap_s
{
#if defined(NO_NATIVE_HEAP_IMPL)
	tag_t *block;
	tag_t *ptr;
	tag_t *end;
#else
	void *handle;
#endif
};

typedef heap_t *heap_p;

/*
Creates a new heap with the requested size.

@param size The size of the heap to create.

@return The new heap, or NULL if creation failed.
*/
heap_p create_heap(size_t size);

/*
Frees a heap allocated with create_heap.

@param heap The heap to free.
*/
void free_heap(heap_p heap);

/*
Allocates memory on a heap.

@param heap The heap the allocate on.
@param size The minimum number of bytes to allocate.

@return A pointer to the newly allocated block, or NULL if allocation fails.
*/
void *halloc(heap_p heap, size_t size);

/*
Frees memory allocated with halloc.

@param heap The heap which was used to allocate the block
@param block The block to free.
*/
void hfree(heap_p heap, void *block);

#endif