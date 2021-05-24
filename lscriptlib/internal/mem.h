#if !defined(MEM_H)
#define MEM_H

#include "heap.h"
#include "object.h"
#include "array.h"

enum
{
	reference_type_object,
	reference_type_array
};

typedef struct reference_s reference_t;
struct reference_s
{
	union
	{
		object_t *object;
		array_t *array;
	};
	unsigned int type;
};

typedef struct manager_s manager_t;
struct manager_s
{
	heap_p heap;
	list_t *refs;
	list_t *strongRefs;
};

manager_t *manager_create(size_t heapsize);
object_t *manager_alloc_object(manager_t *manager, class_t *clazz);
array_t *manager_alloc_array(manager_t *manager, byte_t type, unsigned int length);
reference_t *manager_create_strong_object_reference(manager_t *manager, object_t *object);
reference_t *manager_create_strong_array_reference(manager_t *manager, array_t *array);
void manager_destroy_strong_reference(manager_t *manager, reference_t *reference);
void manager_gc(manager_t *manager, map_t *visibleSet);
void manager_free(manager_t *manager);

#endif