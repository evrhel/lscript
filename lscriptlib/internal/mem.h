#if !defined(MEM_H)
#define MEM_H

#include "heap.h"
#include "object.h"
#include "array.h"

typedef struct manager_s manager_t;
struct manager_s
{
	heap_p heap;
	list_t *refs;
};

manager_t *manager_create(size_t heapsize);
object_t *manager_alloc_object(manager_t *manager, class_t *clazz);
array_t *manager_alloc_array(manager_t *manager, byte_t type, unsigned int length);
void manager_gc(manager_t *manager, map_t *visibleSet);
void manager_free(manager_t *manager);

#endif