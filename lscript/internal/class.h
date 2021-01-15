#if !defined(CLASS_H)
#define CLASS_H

#define CLASS_HASHTABLE_ENTRIES 16

#include "collection.h"
#include "value.h"

typedef signed long long class_flags_t;
typedef signed long long function_flags_t;

enum
{
	CLASS_FLAG_VIRTUAL = 0x1
};

enum
{
	FUNCTION_FLAG_STATIC = 0x1,
	FUNCTION_FLAG_NATIVE = 0x2
};

typedef struct function_s function_t;
typedef struct class_s class_t;

struct function_s
{
	const char *name;
	void *location;
	function_flags_t flags;
	size_t numargs;
	const char **args;
	map_t *argTypes;
	class_t *parentClass;
};

struct class_s
{
	const char *name;		// The class's name
	class_t *super;			// The class's superclass
	class_flags_t flags;	// The class's flags
	const byte_t *data;		// The raw data of the class
	map_t *functions;		// Maps the function names to its location in memory
	map_t *staticFields;	// Maps the static field name to its value in memory
	map_t *fields;			// Maps the field name to its offset
	size_t size;			// Stores the total size this object will allocate
};

typedef class_t *(*classloadproc_t)(const char *classname, void *more);

class_t *class_load(const byte_t *binary, size_t length, classloadproc_t loadproc, void *more);

inline function_t *class_get_function(class_t *clazz, const char *qualifiedName)
{
	return (function_t *)map_at(clazz->functions, qualifiedName);
}

inline value_t *class_get_static_field(class_t *clazz, const char *fieldName)
{
	return (value_t *)map_at(clazz->staticFields, fieldName);
}

inline void *class_get_dynamic_field_offset(class_t *clazz, const char *fieldName)
{
	return map_at(clazz->fields, fieldName);
}

void class_free(class_t *clazz, int freedata);

#endif