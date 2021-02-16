#include "mem.h"

#include "value.h"

#include "mem_debug.h"

#define MARKED_MASK 0x1

static void explore_value(value_t *value);

manager_t *manager_create(size_t heapsize)
{
	manager_t *manager = (manager_t *)MALLOC(sizeof(manager_t));
	if (!manager)
		return NULL;

	manager->heap = create_heap(heapsize);
	if (!manager->heap)
	{
		FREE(manager);
		return NULL;
	}

	manager->refs = list_create();
	if (!manager->refs)
	{
		free_heap(manager->heap);
		FREE(manager);
		return NULL;
	}
	manager->refs->data = (void *)0xbaddcafebaddcafe;

	return manager;
}

object_t *manager_alloc_object(manager_t *manager, class_t *clazz)
{
	size_t size = sizeof(value_t) + clazz->size;
	value_t *value = (value_t *)halloc(manager->heap, size);
	if (!value)
		return NULL;
	value->flags = 0;
	value_set_type(value, lb_object);
	value->ovalue = clazz;
	MEMSET((char *)&value->ovalue + sizeof(lobject), 0, clazz->size);
	list_insert(manager->refs, value);
	return (object_t *)value;
}

array_t *manager_alloc_array(manager_t *manager, byte_t type, unsigned int length)
{
	unsigned int elemSize;
	switch (type)
	{
	case lb_chararray:
	case lb_uchararray:
	case lb_boolarray:
		elemSize = sizeof(lchar);
		break;
	case lb_shortarray:
	case lb_ushortarray:
		elemSize = sizeof(lshort);
		break;
	case lb_intarray:
	case lb_uintarray:
	case lb_floatarray:
		elemSize = sizeof(lint);
		break;
	case lb_longarray:
	case lb_ulongarray:
	case lb_doublearray:
	case lb_objectarray:
		elemSize = sizeof(llong);
		break;
	default:
		return NULL;
		break;
	}
	unsigned int payloadSize = length * elemSize;

	// sizeof(value_t) accounts for flags, length, and dummy fields in array_t
	unsigned int totalSize = payloadSize + sizeof(value_t);

	array_t *array = (array_t *)halloc(manager->heap, totalSize);
	if (!array)
		return NULL;

	array->flags = 0;
	value_set_type((value_t *)array, type);
	array->length = length;
	array->dummy = 0;
	MEMSET(&array->data, 0, payloadSize);
	list_insert(manager->refs, array);

	return array;
}

void manager_gc(manager_t *manager, map_t *visibleSet)
{
	map_iterator_t *mit = map_create_iterator(visibleSet);

	while (mit->node)
	{
		explore_value((value_t *)mit->key);

		mit = map_iterator_next(mit);
	}

	map_iterator_free(mit);

	list_iterator_t *lit = list_create_iterator(manager->refs);
	while (lit)
	{
		list_t *currNode = lit->node;
		if (currNode != manager->refs)
		{
			value_t *value = (value_t *)currNode->data;
			unsigned char *flags = value_manager_flags(value);
			if (!(*flags & MARKED_MASK))
			{
				list_t *prevNode, *nextNode;

				prevNode = currNode->prev;
				nextNode = currNode->next;

				lit = list_iterator_next(lit);

				if (prevNode)
					prevNode->next = nextNode;

				if (nextNode)
					nextNode->prev = prevNode;

				hfree(manager->heap, currNode->data);

				currNode->next = NULL;
				currNode->prev = NULL;
				list_free(currNode, 0);
				continue;
			}
		}
		lit = list_iterator_next(lit);
	}
	list_iterator_free(lit);
}

void manager_free(manager_t *manager)
{
	if (manager)
	{
		free_heap(manager->heap);
		list_free(manager->refs, 0);
		FREE(manager);
	}
}

void explore_value(value_t *value)
{
	if (!value)
		return;

	unsigned char *flags = value_manager_flags(value);
	if (*flags & MARKED_MASK)
		return;

	*flags |= MARKED_MASK;

	object_t *object;
	array_t *array;
	map_iterator_t *fieldIterator;

	unsigned char type = value_typeof(value);
	switch (type)
	{
	case lb_object:
	
		object = (object_t *)value;
		fieldIterator = map_create_iterator(object->clazz->fields);

		while (fieldIterator->node)
		{
			field_t *field = (field_t *)fieldIterator->value;
			unsigned char fieldType = field_typeof(field);
			if (fieldType >= lb_object && fieldType <= lb_objectarray)
			{
				// Explore the object stored in this object
				explore_value(*((value_t **)(((char *)&object->data) + (size_t)field->offset)));
			}
			fieldIterator = map_iterator_next(fieldIterator);
		}

		map_iterator_free(fieldIterator);

		break;
	case lb_objectarray:
		array = (array_t *)value;

		for (luint i = 0; i < array->length; i++)
		{
			object_t *object = (object_t *)array_get_object(array, i);
			explore_value((value_t *)object);
		}

		break;
	}
}
