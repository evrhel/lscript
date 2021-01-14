#include "heap.h"

#include <memory.h>

#include "mem_debug.h"

#define WORD_SIZE sizeof(size_t)
#define HEADER_SIZE WORD_SIZE
#define FOOTER_SIZE WORD_SIZE

#define SHEAP(heap) ((sheap_t*)heap)

#define ALLOCATED_MASK ((size_t)0x1)
#define PREC_ALLOCATED_MASK ((size_t)0x2)
#define SIZE_MASK (~((size_t)0x3))
#define FIELD_MASK ((size_t)0x3)

/*

BLOCK LAYOUT

[HEADER (8 bytes)] [PAYLOAD ...]

HEADER
- 62 bit size field in MSB's
- 1 bit isPrecedingAllocated field in 2nd LSB
- 1 bit isAllocated field in LSB

*/


typedef struct sheap_s sheap_t;
struct sheap_s
{
	size_t *block;
	size_t *ptr;
	size_t *end;
};

static void coalesce_block(sheap_t *heap, size_t *header);

heap_t create_heap(size_t size)
{
	sheap_t *sheap = (sheap_t *)MALLOC(sizeof(sheap_t));
	if (!sheap)
		return NULL;
	size_t adjSize = size + (size % WORD_SIZE);
	const size_t fullSize = adjSize + HEADER_SIZE + FOOTER_SIZE;
	sheap->block = (size_t *)MALLOC(fullSize);
	if (!sheap->block)
	{
		FREE(sheap);
		return NULL;
	}
#if defined(_DEBUG)
	MEMSET(sheap->block, 0xab, fullSize);
#endif
	sheap->block[0] = fullSize << 2;
	sheap->ptr = sheap->block;
	sheap->end = sheap->block + (fullSize / WORD_SIZE);
	*(sheap->end - 1) = fullSize << 2;
	return sheap;
}

void free_heap(heap_t heap)
{
	sheap_t *sheap = SHEAP(heap);
	FREE(sheap->block);
	FREE(heap);
}

void *halloc(heap_t heap, size_t size)
{
	sheap_t *sheap = SHEAP(heap);
	if (size == 0 || size >= ((sheap->end - sheap->block) * WORD_SIZE + HEADER_SIZE + FOOTER_SIZE))
		return NULL;

	size_t desiredSize = size + (size % WORD_SIZE) + HEADER_SIZE + FOOTER_SIZE;

	size_t *start = sheap->ptr;
	do
	{

		if (!(*sheap->ptr & ALLOCATED_MASK)) // Check if isAllocated bit is 0
		{
			size_t blockSize = *sheap->ptr >> 2;
			if (desiredSize <= blockSize)
			{
				size_t *freeBlockFooter = sheap->ptr + (blockSize / WORD_SIZE) - 1;

				*sheap->ptr = desiredSize << 2;
				*sheap->ptr |= ALLOCATED_MASK;

				// If preceding block is allocated, set the allocated bit
				if (sheap->ptr != sheap->block && *(sheap->ptr - 1) & ALLOCATED_MASK)
					*sheap->ptr |= PREC_ALLOCATED_MASK;

				size_t newFreeSize = blockSize - desiredSize;

				size_t *nextHeader = sheap->ptr + (desiredSize / WORD_SIZE);

				if (nextHeader <= sheap->end)
				{
					size_t *nextFooter = nextHeader + ((*nextHeader >> 2) / WORD_SIZE - 1);

					// If we took up the entirety of the free block, the next block must be allocated
					if (newFreeSize > 0)
					{
						size_t flags = (*freeBlockFooter & FIELD_MASK) | PREC_ALLOCATED_MASK;
						size_t newFreeSize = blockSize - desiredSize;
						*freeBlockFooter = (newFreeSize << 2) + flags;

						*nextHeader = *freeBlockFooter;
					}
					else if (nextFooter <= sheap->end)
					{
						// Set the preceding allocated block bit
						*nextHeader |= PREC_ALLOCATED_MASK;
						*nextFooter |= PREC_ALLOCATED_MASK;
					}
				}

				// Set the footer to the value of the header
				*(nextHeader - 1) = *sheap->ptr;

				size_t *resultPtr = sheap->ptr + 1;
				sheap->ptr += desiredSize / 8;
				if (sheap->ptr - 1 == sheap->end)
					sheap->ptr = sheap->block;

#if defined(_DEBUG)
				MEMSET(resultPtr, 0xdd, desiredSize - HEADER_SIZE - FOOTER_SIZE);
#endif
				return resultPtr;
			}
		}

		
	} while (start != sheap->ptr);

	return NULL;
}

void hfree(heap_t heap, void *block)
{

	sheap_t *sheap = SHEAP(heap);

	size_t *sBlock = (size_t *)block;

	if (sBlock <= sheap->block || sBlock >= sheap->end - 1)
	{
		// Invalid free pointer
		return;
	}

	size_t *blockHeader = sBlock - 1;

	size_t blockSize = *blockHeader >> 2;

	size_t *blockFooter = blockHeader + (blockSize / WORD_SIZE) - 1;
	*blockHeader &= ~ALLOCATED_MASK;
	*blockFooter = *blockHeader;

	size_t *nextHeader = blockFooter + 1;
	if (nextHeader < sheap->end)
	{
		size_t nextSize = *nextHeader >> 2;
		*nextHeader &= ~PREC_ALLOCATED_MASK;
		size_t *nextFooter = nextHeader + (nextSize / WORD_SIZE) - 1;
		*nextFooter = *nextHeader;
	}

	coalesce_block(sheap, blockHeader);
}

void coalesce_block(sheap_t *heap, size_t *header)
{
	if (!(*header & PREC_ALLOCATED_MASK) && header != heap->block)
	{
		size_t *prevFooter = header - 1;
		size_t *newHeader = prevFooter - ((*prevFooter >> 2) / WORD_SIZE);
		if (heap->ptr == header)
			heap->ptr = newHeader;
		coalesce_block(heap, newHeader);
		return;
	}

	// Find the next allocated header
	size_t *nextHeader = header + ((*header >> 2) / WORD_SIZE);
	while (nextHeader < heap->end && !(*nextHeader & ALLOCATED_MASK))
		nextHeader += ((*nextHeader >> 2) / WORD_SIZE);

	size_t freeSize = (nextHeader - header) * WORD_SIZE;
	size_t *footer = nextHeader - 1;

	*header &= FIELD_MASK;
	*header += freeSize << 2;
	*footer = *header;

#if defined(_DEBUG)
	MEMSET(header + 1, 0xab, freeSize - HEADER_SIZE - FOOTER_SIZE);
#endif
}
