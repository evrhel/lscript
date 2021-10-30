#if !defined(MEM_H)
#define MEM_H

#include "heap.h"
#include "object.h"
#include "array.h"

enum
{
	reference_type_object,	// The reference is an object
	reference_type_array	// The reference is an array
};

typedef struct reference_s reference_t;
struct reference_s
{
	union
	{
		object_t *object;
		array_t *array;
	};
	unsigned int type;	// The type of object referenced, reference_type_object or reference_type_array
};

typedef struct manager_s manager_t;
struct manager_s
{
	heap_p heap;			// The heap
	list_t *refs;			// A list of all references
	list_t *strongRefs;		// A list of all strong references
};

/*
Creates a new memory manager.

@param heapsize The size of the heap.

@return The new manager, or NULL if the creation failed.
*/
manager_t *manager_create(size_t heapsize);

/*
Allocates an object on the manager's heap.

@param manager The manager on which to allocate the object.
@param clazz The class of the object to allocate.

@return The new object, or NULL if allocation fails.
*/
object_t *manager_alloc_object(manager_t *manager, class_t *clazz);

/*
Allocates an array on the manager's heap.

@param manager The manager on which to allocate the object.
@param type The type of array. Can be one of the types defined in lb.h of the form: lb_[TYPE]array.
@param length The number of elements to allocate on the heap.

@return The new array, or NULL if allocation fails.
*/
array_t *manager_alloc_array(manager_t *manager, byte_t type, unsigned int length);

/*
Creates a new strong reference to an object. As long as an object has at least one strong
reference, it will not be freed on garbage collection. Behavior is undefined if the object
was not allocated on the given manager.

@param manager The manager to create the strong reference on.
@param object The object to create the strong reference to.

@return The new reference, or NULL if the creation fails.
*/
reference_t *manager_create_strong_object_reference(manager_t *manager, object_t *object);

/*
Creates a new strong reference to an array. As long as an object has at least one strong
reference, it will not be freed on garbage collection. Behavior is undefined if the array
was not allocated on the given manager.

@param manager The manager to create the strong reference on.
@param array The array to create the strong reference to.

@return The new reference, or NULL if the creation fails.
*/
reference_t *manager_create_strong_array_reference(manager_t *manager, array_t *array);

/*
Destroys a strong reference.

@param manager The manager on which the strong reference was created.
@param reference The reference to destroy.
*/
void manager_destroy_strong_reference(manager_t *manager, reference_t *reference);

/*
Performs garbage collection on the manager. Any object will be freed unless either of these conditions
are met: an object allocated on the manager is said to be a known reachable value by being in the
visible set or an object has at least one strong reference to it.

@param manager The manager to garbage collect.
@param visibleSet A set of values which are known to be visible - stored in the key field of
each pair.
*/
void manager_gc(manager_t *manager, map_t *visibleSet);

/*
Frees a memory manager allocated with manager_create.

@param manager The manager to free.
*/
void manager_free(manager_t *manager);

#endif