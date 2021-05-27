#if !defined(CLASS_H)
#define CLASS_H

#define CLASS_HASHTABLE_ENTRIES 16

#include "collection.h"
#include "value.h"
#include "debug.h"

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
	const char *name;			// The name of the function
	void *location;				// The location of the function in memory after its declaration
	function_flags_t flags;		// The functions's flags
	size_t numargs;				// The number of arguments this function takes
	const char **args;			// The name of each argument in order
	map_t *argTypes;			// A map from a function argument's name to its type
	class_t *parentClass;		// The function's parent class
	size_t argSize;				// The total number of bytes all the arguments will take up
	size_t references;			// The number of references there are to this function
	byte_t returnType;			// The return type of the function
};

struct class_s
{
	const char *name;		// The class's name
	class_t *super;			// The class's superclass
	class_flags_t flags;	// The class's flags
	byte_t *data;			// The raw data of the class
	map_t *functions;		// Maps the function names to its location in memory
	map_t *staticFields;	// Maps the static field name to its value in memory
	map_t *fields;			// Maps the field name to its offset
	debug_t *debug;			// A pointer to debug information about this class
	size_t size;			// Stores the total size this object will allocate
};

typedef class_t *(*classloadproc_t)(const char *classname, void *more);

/*
Loads a class from its binary data into a class_t.

@param binary The binary class data. The data is not copied and may be changed. Freeing this
during the lifetime of the class results in undefined behavior.
@param length The length of the data.
@param loadproc A pointer to a function which will handle loading any necessary classes when loading
this class. This is called generally when a superclass is needed.
@param more A value which will be passed to loadproc when needed.

@return The new class, or NULL if creation failed.
*/
class_t *class_load(byte_t *binary, size_t length, classloadproc_t loadproc, void *more);

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

/*
Frees a class loaded with class_load.

@param clazz The class to free.
@param freedata Whether to free the binary data associated with the class.
*/
void class_free(class_t *clazz, int freedata);

#endif