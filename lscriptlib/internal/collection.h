#if !defined(COLLECTION_H)
#define COLLECTION_H

#include <stdlib.h>

typedef struct list_s list_t;
struct list_s
{
	void *data;		// Data in the list
	list_t *next;	// Next element in the list
	list_t *prev;	// Previous element in the list
};

typedef struct list_iterator_s list_iterator_t;
struct list_iterator_s
{
	void *data;		// Data at the current location
	list_t *node;	// The current node
};

/*
Hashes a value to a size_t.

@param value The value to hash.

@return The hash as a size_t.
*/
typedef size_t(*hash_func_t)(const void *value);

/*
Compares two values.

@param first The first value to compare.
@param second The second value to compare.

@return Nonzero if the values are equal and 0 otherwise.
*/
typedef char(*compare_func_t)(const void *first, const void *second);

/*
Copies a value to a new location.

@param value The value to copy.

@return A new pointer to a new location with the same data.
*/
typedef void *(*copy_func_t)(const void *value);

/*
Frees a value.

@param value The value to free. Can be NULL.
*/
typedef void (*free_func_t)(const void *value);

typedef struct map_node_s map_node_t;
struct map_node_s
{
	void *key, *value;			// Data
	map_node_t *next, *prev;	// Traversal
};

typedef struct map_s map_t;
struct map_s
{
	map_node_t **table;		// The table storing each entry
	size_t entries;			// The number of entries in the table

	hash_func_t hash;		// The hash function
	compare_func_t compare;	// The comparison function

	copy_func_t keycopy;	// The key copy function
	copy_func_t valuecopy;	// The value copy function

	free_func_t keyfree;	// The key free function
};

typedef struct map_iterator_s map_iterator_t;
struct map_iterator_s
{
	map_t *map;			// The map we are iterating over
	map_node_t *node;	// The current node
	size_t entry;		// The current entry

	void *key, *value;	// Data
};

/*
Creates a new list.

@return The new list, or NULL if creation failed.
*/
list_t *list_create();

/*
Inserts new data at the specified node. The new inserted node will
be after node, such that node->next is the inserted node.

@param node The location to insert the new data.
@param data The data to insert.
*/
void list_insert(list_t *node, const void *data);

/*
Inserts an entire list at the specified node. The list will be
after node, such that node->next is list.

@param node The location to insert the list.
@param list The list to insert.
*/
void list_insert_list(list_t *node, list_t *list);

/*
Removes a singular node from a list.

@param node The node to remove.
@param freeData Whether to free the internal data using free()
*/
void list_remove(list_t *node, int freeData);

/*
Finds the first instance of a node containing data in a list.

@param list The list to find the data in.
@param data The data to find.

@return The node which has a value data, or NULL if not found.
*/
list_t *list_find(list_t *list, const void *data);

/*
Finds the first node in a list, i.e. the first node where list->prev is NULL.

@param list The list to find the start of.

@return The start of the list.
*/
list_t *list_find_start(list_t *list);

/*
Finds the last node in a list, i.e. the first node where list->next is NULL.

@param list The list to find the end of.

@return The end of the list.
*/
list_t *list_find_end(list_t *list);

/*
Creates a new list iterator.

@param list The list to create an iterator over.

@return The new list iterator, or NULL if creation fails.
*/
list_iterator_t *list_create_iterator(list_t *list);

/*
Frees all nodes of a list. Assumes list is the start of the list.

@param list The list to free.
@param freeData Whether to free the data of each node using free().
*/
void list_free(list_t *list, int freeData);

/*
Returns the next element in the list as a new iterator, or NULL if the end
of the list has been reached. No cleanup is needed if NULL is returned.

@param iterator The iterator to increment.

@return The iterator of the next element, or NULL if the last element has been
reached.
*/
list_iterator_t *list_iterator_next(list_iterator_t *iterator);

/*
Frees a list iterator.

@param iterator The iterator to free.
*/
void list_iterator_free(list_iterator_t *iterator);

/*
Creates a new map which maps keys to values using a hash function.

@param entries The number of entries in the map. This is not the total capacity
of the map. Higher values have more memory usage, lower values may result in more
collisions and higher execution time on access/insert functions. Good starting value
is 16.
@param hash The hash function which hashes keys. A value of NULL uses a default hash.
@param compare The compare function which tests key equality. A value of NULL uses a default
compare operator.
@param keycopy The function which copies keys. A value of NULL will only replace the value
of the key and not its contents.
@param valuecopy The function which copies values. A value of NULL will only replace the
value of the value and not its contents.
@param keyfree The function which frees keys. A value of NULL will not free keys.
*/
map_t *map_create(size_t entries, hash_func_t hash,
	compare_func_t compare, copy_func_t keycopy, copy_func_t valuecopy,
	free_func_t keyfree);

/*
Inserts a key-value pair. If an entry already exists with the given key, the value
will be overwritten.

@param map The map to insert the pair into.
@param key The key.
@param value The value.

@return The old value stored at the given key, or NULL if no entry existed.
*/
void *map_insert(map_t *map, const void *key, const void *value);

/*
Removes a key-value pairing with the given key.

@param map The map to remove the pair from.
@param key The key to remove.

@return The old value mapped from the key, or NULL if no entry existed.
*/
void *map_remove(map_t *map, const void *key);

/*
Finds a node with the given key.

@param map The map to find the node in.
@param key The key to search for.

@return The node, or NULL if no pairing exists.
*/
map_node_t *map_find(map_t *map, const void *key);

/*
Returns the value mapped from the key.

@param map The map to get the value at.
@param key The key to get the value of.

@return The value at the given key, or NULL if no entry existed.
*/
void *map_at(map_t *map, const void *key);

/*
Creates a new iterator which iterates over all elements in a map in
no particular order.

@param map The map to create the iterator of.

@return The new iterator, or NULL if creation failed.
*/
map_iterator_t *map_create_iterator(map_t *map);

/*
Frees a map.

@param map The map to free.
@param freeData Whether to free the data of the value using free().
*/
void map_free(map_t *map, int freeData);

/*
Returns the next element in an iterator. The last element has been reached
when the return value's field node is NULL. Must always be freed using
map_iterator_free.

@param iterator The map iterator to increment.

@return iterator
*/
map_iterator_t *map_iterator_next(map_iterator_t *iterator);

/*
Frees a map iterator.

@param iterator The iterator to free.
*/
void map_iterator_free(map_iterator_t *iterator);

/*
A string hash function for a map when using string keys.

@param string The string to hash.

@return The hash.
*/
size_t string_hash_func(const char *string);

/*
A string comparison function for a map when using string keys.

@param first The first NULL-terminated string to compare.
@param second The second NULL-terminated string to compare.

@return Nonzero if all characters in each string are equal.
*/
char string_compare_func(const char *first, const char *second);

/*
String copy function for a map when using string keys.

@param string The string to copy.

@return The copied string.
*/
void *string_copy_func(const char *string);

#endif