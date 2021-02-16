#if !defined(COLLECTION_H)
#define COLLECTION_H

#include <stdlib.h>

typedef struct list_s list_t;
struct list_s
{
	void *data;
	list_t *next;
	list_t *prev;
};

typedef struct list_iterator_s list_iterator_t;
struct list_iterator_s
{
	void *data;
	list_t *node;
};

typedef size_t(*hash_func_t)(const void *value);
typedef char(*compare_func_t)(const void *first, const void *second);
typedef void *(*copy_func_t)(const void *value);
typedef void (*free_func_t)(const void *value);

typedef struct map_node_s map_node_t;
struct map_node_s
{
	void *key, *value;
	map_node_t *next, *prev;
};

typedef struct map_s map_t;
struct map_s
{
	map_node_t **table;
	size_t entries;

	hash_func_t hash;
	compare_func_t compare;

	copy_func_t keycopy;
	copy_func_t valuecopy;

	free_func_t keyfree;
};

typedef struct map_iterator_s map_iterator_t;
struct map_iterator_s
{
	map_t *map;
	map_node_t *node;
	size_t entry;

	void *key, *value;
};

list_t *list_create();
void list_insert(list_t *node, const void *data);
void list_insert_list(list_t *node, list_t *list);
void list_remove(list_t *node, int freeData);
list_t *list_find(list_t *list, const void *data);
list_t *list_find_start(list_t *list);
list_t *list_find_end(list_t *list);
list_iterator_t *list_create_iterator(list_t *list);
void list_free(list_t *list, int freeData);

list_iterator_t *list_iterator_next(list_iterator_t *iterator);
void list_iterator_free(list_iterator_t *iterator);

map_t *map_create(size_t entries, hash_func_t hash,
	compare_func_t compare, copy_func_t keycopy, copy_func_t valuecopy,
	free_func_t keyfree);
void *map_insert(map_t *map, const void *key, const void *value);
void *map_remove(map_t *map, const void *key);
map_node_t *map_find(map_t *map, const void *key);
void *map_at(map_t *map, const void *key);
map_iterator_t *map_create_iterator(map_t *map);
void map_free(map_t *map, int freeData);

map_iterator_t *map_iterator_next(map_iterator_t *iterator);
void map_iterator_free(map_iterator_t *iterator);

size_t string_hash_func(const char *string);
char string_compare_func(const char *first, const char *second);
void *string_copy_func(const char *string);

#endif