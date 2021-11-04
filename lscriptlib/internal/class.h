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
	FUNCTION_FLAG_NATIVE = 0x2,
	FUNCTION_FLAG_ABSTRACT = 0x4
};

typedef struct function_s function_t;
typedef struct class_s class_t;

struct function_s
{
	const char *name;			// The name of the function
	const char *qualifiedName;	// The qualified name of the function
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
	char *safeName;			// The class's safe name ('.' -> '_')
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
@param loadSuperclasses Whether to recursively load superclasses using loadproc.
@param loadproc A pointer to a function which will handle loading any necessary classes when loading
this class. This is called generally when a superclass is needed.
@param more A value which will be passed to loadproc when needed.

@return The new class, or NULL if creation failed.
*/
class_t *class_load(byte_t *binary, size_t length, int loadSuperclasses, classloadproc_t loadproc, void *more);

/*
Sets a class' superclass. The class must not already have a superclass.

clazz and superclass must point to different classes. If they are the same, behavior is undefined.

@param clazz The class that will be given a superclass.
@param superclass The superclass to set.

@return nonzero on success.
*/
int set_superclass(class_t *__restrict clazz, class_t *__restrict superclass);

/*
Returns a member function of a class by its qualified name.

@param clazz The class to fetch the function from.
@param qualifiedName The qualified name of the function.

@return THe respective function, or NULL if it doesn't exist.
*/
inline function_t *class_get_function(class_t *clazz, const char *qualifiedName)
{
	return (function_t *)map_at(clazz->functions, qualifiedName);
}

/*
Returns a pointer to a static field of a class by its name.

@param clazz The class to fetch the field from.
@param fieldName The name of the field.

@return The respective field, or NULL if it doesn't exist or is nonstatic.
*/
inline value_t *class_get_static_field(class_t *clazz, const char *fieldName)
{
	return (value_t *)map_at(clazz->staticFields, fieldName);
}

/*
Returns the offset of a dynamic field of a class by its name.

@param clazz The class to fetch the field from.
@param fieldName The name of the field.

@return The respective field's offset, or NULL if it doesn't exist or is not dynamic.
*/
inline void *class_get_dynamic_field_offset(class_t *clazz, const char *fieldName)
{
	return map_at(clazz->fields, fieldName);
}

/*
Frees a class loaded with class_load.

@param clazz The class to free.
@param freedata Whether to free the binary data associated with the class.
*/
void class_free(class_t *__restrict clazz, int freedata);

const char *class_get_last_error();

#endif