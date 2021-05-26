#if !defined(VM_H)
#define VM_H

#include "mem.h"
#include "types.h"
#include "datau.h"
#include <stdarg.h>

#if defined(_WIN32)
#include <Windows.h>
#else
#error Unsupported platform
#endif

#define MAX_EXCEPTION_STRING_LENGTH 100

typedef struct vm_s vm_t;
typedef struct env_s env_t;
typedef unsigned long long vm_flags_t;

enum
{
	exception_none = 0,
	exception_out_of_memory,
	exception_stack_overflow,
	exception_bad_command,
	exception_vm_error,
	exception_illegal_state,
	exception_class_not_found,
	exception_function_not_found,
	exception_field_not_found,
	exception_null_dereference,
	exception_bad_variable_name,
	exception_bad_array_index,
	exception_link_error
};

enum
{
	vm_flag_verbose =			0x1,
	vm_flag_no_load_debug =		0x2,
	vm_flag_verbose_errors =	0x4
};

struct vm_s
{
	list_t *envs;
	list_t *envsLast;
	map_t *classes;
	manager_t *manager;
	size_t stackSize;
	map_t *properties;

	list_t *paths;

	map_t *loadedClassObjects;

#if defined(WIN32)
	HMODULE *hLibraries;
	HANDLE hVMThread;
	DWORD dwVMThreadID;
	
	DWORD dwPadding;
#else
#endif
	size_t libraryCount;

	vm_flags_t flags;
};

struct env_s
{
	byte_t *rip;
	byte_t *cmdStart;
	vm_t *vm;

	byte_t *stack;
	byte_t *rsp, *rbp;

	list_t *variables;

	int exception;
	char *exceptionMessage;

	union
	{
		byte_t bret;
		word_t wret;
		dword_t dret;
		qword_t qret;
		real4_t r4ret;
		real8_t r8ret;
		void *vret;
	};
};

char *new_exception_stringv(const char *format, va_list ls);
inline char *new_exception_string(const char *format, ...)
{
	va_list ls;
	va_start(ls, format);
	char *result = new_exception_stringv(format, ls);
	va_end(ls);
	return result;
}
void free_exception_string(const char *exceptionString);

/*
Creates a new virtual machine with the designated parameters.

@param heapSize The size of the heap.
@param stackSize The size of the stack, per thread.
@param flags Creation flags.
@param pathCount The number of paths.
@param paths The paths for which the virtual machine will search for classes on.

@return The new virtual machine, or NULL if creation failed.
*/
vm_t *vm_create(size_t heapSize, size_t stackSize, void *lsAPILib, vm_flags_t flags, int pathCount, const char *const paths[]);

/*
Starts the virtual machine on a main function with the given arguments.

@parma vm The virtual machine to start.
@param startOnNewThread Whether to start the virtual machine on a separate thread.
@param argc The number of command line arguments.
@param argv The command line arguments to be passed to main.

@return 0 if the start was successful.
*/
int vm_start(vm_t *vm, int startOnNewThread, int argc, const char *const argv[]);

/*
Finds a class with the requested classname if it is loaded into the virtual machine.

@param vm The virtual machine to find the class on.
@param classname The name of the class to find.

@return The respective class, or NULL if it was not found.
*/
class_t *vm_get_class(vm_t *vm, const char *classname);

/*
Loads a class with the requested classname if it is not loaded already. The virtual machine
will search on the classpath for a class of the requested name.

@param vm The virtual machine to load the class into.
@param classname The name of the class to load.

@return The respective class, if it already is loaded or was loaded successfully, or NULL
if the load failed.
*/
class_t *vm_load_class(vm_t *vm, const char *classname);

/*
Loads a class from a file onto the virtual machine.

@param vm The virtual machine to load the class into.
@param filename The filepath of the class.

@return The loaded class, or NULL if the load failed.
*/
class_t *vm_load_class_file(vm_t *vm, const char *filename);

/*
Loads a class from binary data onto the virtual machine.

@param vm The virtual machine to load the class into.
@param binary A pointer to the binary data of the class.
@param size The size of the binary data, in bytes.

@return The loaded class, or NULL if the load failed.
*/
class_t *vm_load_class_binary(vm_t *vm, byte_t *binary, size_t size);

/*
Returns an instance of a Class LScript class for the requested classname.

@param vm The virtual machine on which the class is loaded.
@param classname The classname.

@return The respective Class object, or NULL if no such class exists.
*/
object_t *vm_get_class_object(vm_t *vm, const char *classname);

/*
Adds a path to the classpath.

@param vm The virtual machine to add the path to.
@param path The path to add.
*/
void vm_add_path(vm_t *vm, const char *path);

/*
Loads a native library onto the virtual machine. The name of the library
should not include native extensions (such as .dll).

@param vm The virtual machine to load the library onto.
@param libpath The path of the native library, excluding native extensions.

@return 1 if the load succeeded and 0 otherwise.
*/
int vm_load_library(vm_t *vm, const char *libpath);

/*
Frees the virtual machine.

@param vm The virtual machine to free.
@param threadWaitTime The maximum amount of time to wait before forcefully shutting down
any threads, in milliseconds.
*/
void vm_free(vm_t *vm, unsigned long threadWaitTime);

/*
Creates a new execution environment in the specified virtual machine.

@return The new enviornment, or NULL if creation failed.
*/
env_t *env_create(vm_t *vm);

/*
Resolves a variable name in the current scope. If the find fails, an exception will be
raised in the environment.

@param env The environment to resolve the variable in.
@param name The name of the variable.
@param data A pointer to a pointer which will point to the data stored in the variable on success.
@param flags A pointer to a flags_t which will store the flags carried by the variable on success.

@return 1 on success and 0 otherwise.
*/
int env_resolve_variable(env_t *env, const char *name, data_t **data, flags_t *flags);

/*
Resolves an object field in an environment. If the find fails, an exception will be
raised in the environment.

@param env The environment to resolve the field in.
@param object The source object.
@param data A pointer to a pointer which will point to the data stored in the field on success.
@param flags A pointer to a flags_t which will store the flags carried by the field on success.

@return 1 on success and 0 otherwise.
*/
int env_resolve_object_field(env_t *env, object_t *object, const char *name, data_t **data, flags_t *flags);

/*
Resolves a function name in the current scope. If the resolution fails, an exception will
be reaised in the environment.

@param env The environment to resolve the function in.
@param name The name of the function to resolve. This can have local variables and class names preceding
the actual function name, delimited by '.'s.
@param function A pointer to a pointer which will point to the respective function on success.

@return 1 on success and 0 otherwise.
*/
int env_resolve_function_name(env_t *env, const char *name, function_t **function);

/*
Resolves a dynamic function name in the current scope, fetching the object to call the function
on, its flags, and the function pointer. If the resolution fails, an exception will be raised in
the environment.

@param env The environment to resolve the function in.
@param name The name of the function to resolve. This should have variables preceding the function name
as it resolves a dynamic (non-static) function.
@param function A pointer to a pointer which will point to the respective function on success.
@param data A pointer to a pointer which will contain the object to call the function on on success.
@param flags A pointer to a flags_t which will contain the object flags on success.
*/
int env_resolve_dynamic_function_name(env_t *env, const char *name, function_t **function, data_t **data, flags_t *flags);

/*
Runs a function declared static in an execution environment.

@param env The environment to run in.
@param function The function to run.
@param ls The function arguments.

@return Any exception raised by the function, or exception_none if there was none raised.
*/
int env_run_func_staticv(env_t *env, function_t *function, va_list ls);

/*
Runs a function declared dynamic in an execution environment.

@param env The environment to run in.
@param function The function to run.
@param object The object to run the function on.
@param ls The function arguments.

@return Any exception raised by the function, or exception_none if there was none raised.
*/
int env_run_funcv(env_t *env, function_t *function, object_t *object, va_list ls);

/*
Creates a new String object on the virtual machine heap from a C-style string.

@param env The environment to create the string in.
@param cstring A C-style string to populate the new object with.

@return The new String object, or NULL if creation failed.
*/
object_t *env_new_string(env_t *env, const char *cstring);

/*
Creates a new array of Strings on the virtual machine heap from an array of
C-style strings.

@param env The environment to create the strings in.
@param count The number of strings to create.
@param strings An array of C-style strings to populate each respective element in the
array with.

@return An objectarray of length count containing String objects, or NULL if
creation failed.
*/
array_t *env_new_string_array(env_t *env, unsigned int count, const char *const strings[]);

/*
Raises an exception on an environment.

@param env The environment to raise the exception in.
@param exception The exception to raise.
@param format A formatted message.
@param ls The values which will replace each argument in format.

@return The exception raised.
*/
int env_raise_exceptionv(env_t *env, int exception, const char *format, va_list ls);

/*
Raises an exception on an environment.

@param env The environment to raise the exception in.
@param exception The exception to raise.
@param format A formatted message.
@param ... The values which will replace each argument in format.

@return The exception raised.
*/
inline int env_raise_exception(env_t *env, int exception, const char *format, ...)
{
	va_list ls;
	va_start(ls, format);
	int result = env_raise_exceptionv(env, exception, format, ls);
	va_end(ls);
	return result;
}

/*
Returns the data surrounding an exception if one was raised.

@param env The environment to get the data from.
@param function A pointer to a function pointer which will point to the function in which
the exception was raised.
@param location A pointer to a void pointer which will point to the execution location
when the exception was raised.

@return The exception which was raised.
*/
int env_get_exception_data(env_t *env, function_t **function, void **location);

/*
Runs a function declared static in an execution environment.

@param env The environment to run in.
@param function The function to run.
@param ... The function arguments.

@return Any exception raised by the function, or exception_none if there was none raised.
*/
inline int env_run_func_static(env_t *env, function_t *function, ...)
{
	va_list ls;
	va_start(ls, function);
	int result = env_run_func_staticv(env, function, ls);
	va_end(ls);
	return result;
}

/*
Runs a function declared dynamic in an execution environment.

@param env The environment to run in.
@param function The function to run.
@param object The object to run the function on.
@param ... The function arguments.

@return Any exception raised by the function, or exception_none if there was none raised.
*/
inline int env_run_func(env_t *env, function_t *function, object_t *object, ...)
{
	va_list ls;
	va_start(ls, object);
	int result = env_run_funcv(env, function, object, ls);
	va_end(ls);
	return result;
}

/*
Frees an execution environment allocated with env_create.

@param env The environment to free.
*/
void env_free(env_t *env);

#endif