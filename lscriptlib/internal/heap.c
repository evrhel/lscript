#include "heap.h"

#if defined(_WIN32)
#include <Windows.h>
#endif

#include <memory.h>

#include "mem_debug.h"

#define WORD_SIZE sizeof(tag_t)
#define HEADER_SIZE WORD_SIZE
#define FOOTER_SIZE WORD_SIZE

#define SHEAP(heap) ((sheap_t*)heap)

#define ALLOCATED_MASK ((tag_t)0x1)
#define PREC_ALLOCATED_MASK ((tag_t)0x2)
#define SIZE_MASK (~((tag_t)0x3))
#define FIELD_MASK ((tag_t)0x3)

#define GET_BLOCK_SIZE(tag) ((tag_t)((tag)>>2))
#define SET_BLOCK_SIZE(tagPtr, size) (*tagPtr=(((*tagPtr)&FIELD_MASK)+((size)<<2)))

/*

BLOCK LAYOUT

[HEADER (8 bytes)] [PAYLOAD ...]

HEADER
- 62 bit size field in MSB's
- 1 bit isPrecedingAllocated field in 2nd LSB
- 1 bit isAllocated field in LSB

*/

static void coalesce_block(heap_p heap, tag_t *header);

heap_p create_heap(size_t size)
{
	heap_t *heap = (heap_t *)MALLOC(sizeof(heap_t));
	if (!heap)
		return NULL;
	size_t adjSize = size + (size % WORD_SIZE);
	const size_t fullSize = adjSize + HEADER_SIZE + FOOTER_SIZE;
#if defined(_WIN32)
	SYSTEM_INFO sSysInfo;
	GetSystemInfo(&sSysInfo);
	DWORD dwPageSize = sSysInfo.dwPageSize;
	heap->block = (size_t *)HeapAlloc(GetProcessHeap(), 0, fullSize);
#else
#endif
	if (!heap->block)
	{
		FREE(heap);
		return NULL;
	}
#if defined(_DEBUG)
	memset(heap->block, 0xab, fullSize);
#endif
	heap->block[0] = fullSize << 2;
	heap->ptr = heap->block;
	heap->end = heap->block + (fullSize / WORD_SIZE);
	*(heap->end - 1) = fullSize << 2;
	return heap;
}

void free_heap(heap_p heap)
{
#if defined(_WIN32)
	HeapFree(GetProcessHeap(), 0, heap->block);
#else
#endif
	FREE(heap);
}

void *halloc(heap_p heap, size_t size)
{
	// Make the size 8-byte aligned
	size_t desiredSize;
	if (size % WORD_SIZE)
		desiredSize = ((size / 8) * 8) + WORD_SIZE + HEADER_SIZE + FOOTER_SIZE;
	else
		desiredSize = size + HEADER_SIZE + FOOTER_SIZE;

	if (size == 0 || size >= ((heap->end - heap->block) * WORD_SIZE))
		return NULL;

	tag_t *start = heap->ptr;
	do
	{

		if (!(*heap->ptr & ALLOCATED_MASK)) // Check if isAllocated bit is 0
		{
			size_t freeBlockSize = GET_BLOCK_SIZE(*heap->ptr); // The size of the free block
			if (desiredSize <= freeBlockSize)
			{
				size_t *freeBlockFooter = heap->ptr + (freeBlockSize / WORD_SIZE) - 1;

				//*sheap->ptr = desiredSize << 2;
				SET_BLOCK_SIZE(heap->ptr, desiredSize);
				*heap->ptr |= ALLOCATED_MASK;

				// If preceding block is allocated, set the allocated bit
				if (heap->ptr != heap->block && *(heap->ptr - 1) & ALLOCATED_MASK)
					*heap->ptr |= PREC_ALLOCATED_MASK;

				size_t newFreeSize = freeBlockSize - desiredSize;

				tag_t *nextHeader = heap->ptr + (desiredSize / WORD_SIZE);

				if (nextHeader <= heap->end)
				{
					//tag_t *nextFooter = nextHeader + (newFreeSize / WORD_SIZE) - 1;

					// If we took up the entirety of the free block, the next block must be allocated
					if (newFreeSize > 0)
					{
						tag_t flags = (*freeBlockFooter & FIELD_MASK) | PREC_ALLOCATED_MASK;
						size_t newFreeSize = freeBlockSize - desiredSize;
						*freeBlockFooter = flags;
						SET_BLOCK_SIZE(freeBlockFooter, newFreeSize);
						//*freeBlockFooter = (newFreeSize << 2) + flags;

						*nextHeader = *freeBlockFooter;
					}
					else if (freeBlockFooter <= heap->end)
					{
						// Set the preceding allocated block bit
						*nextHeader |= PREC_ALLOCATED_MASK;
						*freeBlockFooter |= PREC_ALLOCATED_MASK;
					}
				}

				// Set the footer to the value of the header
				*(nextHeader - 1) = *heap->ptr;

				tag_t *resultPtr = heap->ptr + 1;
				heap->ptr += desiredSize / 8;
				if (heap->ptr - 1 == heap->end)
					heap->ptr = heap->block;

#if defined(_DEBUG)
				memset(resultPtr, 0xcd, size);
#endif
				return resultPtr;
			}
		}

		
	} while (start != heap->ptr);

	return NULL;
}

void hfree(heap_p heap, void *block)
{
	if (!block)
		return;

	tag_t *sBlock = (tag_t *)block;

	if (sBlock <= heap->block || sBlock >= heap->end - 1)
	{
		// Invalid free pointer
#if defined(_DEBUG) && defined(_WIN32)
		MessageBoxA(NULL, "Free pointer is outside of heap range!", "Invalid free pointer", MB_OK);
		RaiseException(EXCEPTION_BREAKPOINT, 0, 0, NULL);
#endif
		return;
	}

	tag_t *blockHeader = sBlock - 1;

	size_t blockSize = *blockHeader >> 2;

	tag_t *blockFooter = blockHeader + (blockSize / WORD_SIZE) - 1;

#if defined(_DEBUG) && defined(_WIN32)
	if (*blockFooter != *blockHeader)
	{
		MessageBoxA(NULL, "Heap corruption detected:\nheader != footer!", "Heap corruption", MB_OK | MB_ICONERROR);
		RaiseException(EXCEPTION_BREAKPOINT, 0, 0, NULL);
		return;
	}
#endif

	*blockHeader &= ~ALLOCATED_MASK;
	*blockFooter = *blockHeader;

	tag_t *nextHeader = blockFooter + 1;
	if (nextHeader < heap->end)
	{
		size_t nextSize = *nextHeader >> 2;
		*nextHeader &= ~PREC_ALLOCATED_MASK;
		tag_t *nextFooter = nextHeader + (nextSize / WORD_SIZE) - 1;
		*nextFooter = *nextHeader;
	}

	coalesce_block(heap, blockHeader);
}

void coalesce_block(heap_p heap, tag_t *header)
{
	if (!(*header & PREC_ALLOCATED_MASK) && header != heap->block)
	{
		tag_t *prevFooter = header - 1;
		size_t prevBlockSize = GET_BLOCK_SIZE(*prevFooter);
		tag_t *newHeader = prevFooter - (prevBlockSize / WORD_SIZE) + 1;
		if (heap->ptr == header)
			heap->ptr = newHeader;
		coalesce_block(heap, newHeader);
		return;
	}

	// Find the next allocated header
	size_t blockSize = GET_BLOCK_SIZE(*header);
	tag_t *nextHeader = header + (blockSize / WORD_SIZE);
	while (nextHeader < heap->end && !(*nextHeader & ALLOCATED_MASK))
		nextHeader += (GET_BLOCK_SIZE(*nextHeader) / WORD_SIZE);

	size_t freeSize = (nextHeader - header) * WORD_SIZE;
	tag_t *footer = nextHeader - 1;

	*header &= FIELD_MASK;
	SET_BLOCK_SIZE(header, freeSize);
	//*header += freeSize << 2;
	*footer = *header;

#if defined(_DEBUG)
	memset(header + 1, 0xab, freeSize - HEADER_SIZE - FOOTER_SIZE);
#endif
}
