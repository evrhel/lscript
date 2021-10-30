#include "collection.h"

#include <string.h>
#include <assert.h>

#include "mem_debug.h"

list_t *list_create()
{
    return (list_t *)CALLOC(1, sizeof(list_t));
}

void list_insert(list_t *node, const void *data)
{
    list_t *listNext = node->next;

    list_t *nextNode = list_create();

    node->next = nextNode;
    nextNode->prev = node;
    nextNode->data = (void *)data;
    
    if (listNext)
        listNext->prev = nextNode;
}

void list_insert_list(list_t *node, list_t *list)
{
    list_t *nodeNext = node->next;
    list_t *listEnd;

    listEnd = list_find_end(list);

    node->next = list;
    listEnd->next = nodeNext;
    nodeNext->prev = listEnd;
}

void list_remove(list_t *node, int freeData)
{
    list_t *nodePrev = node->prev;
    list_t *nodeNext = node->next;

    if (nodePrev)
        nodePrev->next = nodeNext;

    if (nodeNext)
        nodeNext->prev = nodePrev;

    if (freeData)
    {
        FREE(node->data);
        node->data = NULL;
    }
    FREE(node);
}

list_t *list_find(list_t *list, const void *data)
{
    while (list)
    {
        if (list->data == data)
            return list;
        list = list->next;
    }
    return NULL;
}

list_t *list_find_start(list_t *list)
{
    assert(list);

    while (list->prev)
        list = list->prev;
    return list;
}

list_t *list_find_end(list_t *list)
{
    assert(list);

    while (list->next)
        list = list->next;
    return list;
}

list_t *list_copy(list_t *src, copy_func_t copyFunc)
{
    list_t *result = list_create();
    if (!result)
        return NULL;

    result->data = copyFunc ? copyFunc(src->data) : src->data;

    list_t *curr = result->next;
    src = src->next;
    while (src)
    {
        list_insert(curr, copyFunc ? copyFunc(src->data) : src->data);
        src = src->next;
        curr = curr->next;
    }

    return result;
}

list_iterator_t *list_create_iterator(list_t *list)
{
    if (list == NULL) return NULL;

    list_iterator_t *iterator = (list_iterator_t *)MALLOC(sizeof(list_iterator_t));
    if (!iterator)
        return NULL;

    iterator->data = list->data;
    iterator->node = list;

    return iterator;
}

void list_free(list_t *list, int freeData)
{
    if (!list)
        return;
    list_t *next;
    while (list)
    {
        next = list->next;
        if (freeData)
        {
            FREE(list->data);
            list->data = NULL;
        }
        FREE(list);
        list = next;
    }
}

list_iterator_t *list_iterator_next(list_iterator_t *iterator)
{
    list_t *next = iterator->node->next;
    if (!next)
    {
        FREE(iterator);
        return NULL;
    }

    list_iterator_t *result = list_create_iterator(next);
    FREE(iterator);

    return result;
}

void list_iterator_free(list_iterator_t *iterator)
{
    FREE(iterator);
}

map_t *map_create(size_t entries, hash_func_t hash, compare_func_t compare, copy_func_t keycopy, copy_func_t valuecopy,
    free_func_t keyfree)
{
    map_t *map = (map_t *)MALLOC(sizeof(map_t));
    if (!map)
        return NULL;
    map->table = (map_node_t **)CALLOC(entries, sizeof(map_node_t *));
    if (!map->table)
    {
        FREE(map);
        return NULL;
    }

    map->entries = entries;
    map->hash = hash;
    map->compare = compare;

    map->keycopy = keycopy;
    map->valuecopy = valuecopy;
    map->keyfree = keyfree;

    return map;
}

void *map_insert(map_t *map, const void *key, const void *value)
{
    size_t hash = map->hash ? map->hash(key) : (size_t)key;
    size_t index = hash % map->entries;
    void *prevVal;

    if (!map->table[index])
    {
        map->table[index] = (map_node_t *)MALLOC(sizeof(map_node_t));
        if (!map->table[index])
            return NULL;
        map->table[index]->next = NULL;
        map->table[index]->prev = NULL;
        map->table[index]->key = map->keycopy ? map->keycopy(key) : (void *)key;
        map->table[index]->value = map->valuecopy ? map->valuecopy(value) : (void *)value;
        return NULL;
    }
    else
    {
        map_node_t *node = map->table[index];
        map_node_t *prev = NULL;
        while (node)
        {
            if (map->compare ? map->compare(node->key, key) : node->value == value)
            {
                if (node->value == value)
                    return node->value;
                prevVal = node->value;
                node->value = map->valuecopy ? map->valuecopy(value) : (void *)value;
                return prevVal;
            }

            prev = node;
            node = node->next;
        }

        node = prev->next = (map_node_t *)MALLOC(sizeof(map_node_t));
        if (!node)
            return NULL;
        node->next = NULL;
        node->prev = prev;
        node->key = map->keycopy ? map->keycopy(key) : (void *)key;
        node->value = map->valuecopy ? map->valuecopy(value) : (void *)value;
        return NULL;
    }
}

void *map_remove(map_t *map, const void *key)
{
    map_node_t *node = map_find(map, key);
    if (!node)
        return NULL;
    map_node_t *nodeNext = node->next;
    map_node_t *nodePrev = node->prev;

    if (nodePrev)
        nodePrev->next = nodeNext;
    else
    {
        size_t hash = map->hash ? map->hash(key) : (size_t)key;
        size_t index = hash % map->entries;
        map->table[index] = nodeNext;
    }
    
    if (nodeNext)
        nodeNext->prev = nodePrev;

    if (map->keyfree)
        map->keyfree(node->key);
    void *prevVal = node->value;
    FREE(node);
    return prevVal;
}

map_node_t *map_find(map_t *map, const void *key)
{
    size_t hash = map->hash ? map->hash(key) : (size_t)key;
    size_t index = hash % map->entries;
    if (!map->table[index])
        return NULL;
    else
    {
        map_node_t *node = map->table[index];
        while (node)
        {
            if (map->compare && map->compare(node->key, key))
            {
                return node;
            }
            else if (node->key == key)
            {
                return node;
            }
            
            node = node->next;
        }
        return NULL;
    }
}

void *map_at(map_t *map, const void *key)
{
    map_node_t *node = map_find(map, key);
    return node ? node->value : NULL;
}

map_t *map_copy(map_t *map, copy_func_t copyFunc)
{
    if (!map)
        return NULL;

    map_t *result = map_create(map->entries, map->hash, map->compare, map->keycopy, map->valuecopy, map->keyfree);
    if (!result)
        return NULL;

    map_iterator_t *mit = map_create_iterator(map);
    if (!mit)
        return NULL;

    while (mit->node)
    {
        map_insert(result, mit->key, mit->value);
        mit = map_iterator_next(mit);
    }
    map_iterator_free(mit);

    return result;
}

map_iterator_t *map_create_iterator(map_t *map)
{
    map_iterator_t *iterator = (map_iterator_t *)CALLOC(1, sizeof(map_iterator_t));
    if (!iterator)
        return NULL;
    iterator->map = map;
    while (iterator->entry < map->entries)
    {
        if (iterator->node)
            break;
        iterator->node = map->table[iterator->entry++];
    }
    if (iterator->node)
    {
        iterator->key = iterator->node->key;
        iterator->value = iterator->node->value;
    }
    else
    {
        iterator->map = NULL;
        iterator->node = NULL;
        iterator->entry = 0;
    }
    return iterator;
}

void map_free(map_t *map, int freeData)
{
    if (!map)
        return;
    void *prev;
    for (size_t i = 0; i < map->entries; i++)
    {
        map_node_t *node = map->table[i];
        while (node)
        {
            if (freeData)
            {
                FREE(node->value);
                node->value = NULL;
            }

            prev = node;
            node = node->next;
            FREE(prev);
        }
        map->table[i] = NULL;
    }
    FREE(map->table);
    map->table = NULL;

    FREE(map);
}

map_iterator_t *map_iterator_next(map_iterator_t *iterator)
{
    if (!iterator->node)
        return iterator;

    iterator->node = iterator->node->next;
    if (iterator->node)
    {
        iterator->key = iterator->node->key;
        iterator->value = iterator->node->value;
    }
    else
    {
        while (iterator->entry < iterator->map->entries)
        {
            if (iterator->node)
                break;
            iterator->node = iterator->map->table[iterator->entry++];
        }
        if (iterator->node)
        {
            iterator->key = iterator->node->key;
            iterator->value = iterator->node->value;
        }
        else
        {
            iterator->map = NULL;
            iterator->node = NULL;
            iterator->entry = 0;
        }
    }

    return iterator;
}

void map_iterator_free(map_iterator_t *iterator)
{
    FREE(iterator);
}

size_t string_hash_func(const char *string)
{
    size_t hash = 0;
    while (*string)
    {
        hash++;
        hash *= (size_t)(*string) + 1ULL;
        string++;
    }
    return hash;
}

char string_compare_func(const char *first, const char *second)
{
    return !strcmp(first, second);
}

void *string_copy_func(const char *string)
{
    if (!string)
        return NULL;
    size_t len = strlen(string) + 1;
    char *nstring = (char *)MALLOC(len);
    if (!nstring)
        return NULL;
    MEMCPY(nstring, string, len);
    return nstring;
}
