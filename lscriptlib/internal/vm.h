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

/*
The maximum length of an exception string.
*/
#define MAX_EXCEPTION_STRING_LENGTH 100

#define EMSGLEN 64
#define HISTLEN 64

#define CLASS_CLASSNAME "lscript.lang.Class"
#define OBJECT_CLASSNAME "lscript.lang.Object"
#define STRING_CLASSNAME "lscript.lang.String"

/*
Checks if verbose error messages should be used given flags.
*/
#define DO_VERBOSE_ERR(flags) (((flags)&vm_flag_verbose)||((flags)&vm_flag_verbose_errors))

typedef struct vm_s vm_t;
typedef struct vm_snapshot_s vm_snapshot_t;
typedef struct env_snapshot_s env_snapshot_t;
typedef struct env_s env_t;
typedef unsigned long long vm_flags_t;

enum
{
	exception_none = 0,				// No exception
	exception_out_of_memory,		// No more memory is avaliable for heap allocation
	exception_stack_overflow,		// No more memory is avaliable on the stack
	exception_bad_command,			// A command was malformed
	exception_vm_error,				// An error occurred in the virtual machine
	exception_illegal_state,		// The virtual machine was put into an illegal state
	exception_class_not_found,		// A class name was not able to be resolved
	exception_function_not_found,	// A function name was not able to be resolved
	exception_field_not_found,		// A field name was not able to be resolved
	exception_null_dereference,		// An attempt was made to access data of a null object
	exception_bad_variable_name,	// A variable name was not able to be resolved
	exception_bad_array_index,		// An array index was out of bounds
	exception_link_error			// A native function failed to be linked
};

enum
{
	vm_flag_verbose =			0x1,	// Print verbose output
	vm_flag_no_load_debug =		0x2,	// Don't load debugging symbols
	vm_flag_verbose_errors =	0x4		// Print verbose error output
};

struct vm_s
{
	list_t *envs;				// List of all execution environments
	list_t *envsLast;			// The last environment in envs
	map_t *classes;				// A map which maps class names to class structures
	manager_t *manager;			// The memory manager
	size_t stackSize;			// The stack size for each execution environment
	map_t *properties;			// The properties of the virtual machine

	list_t *paths;				// The classpaths

	map_t *loadedClassObjects;	// A map which maps class names to Class object instances

#if defined(WIN32)
	HMODULE *hLibraries;		// Loaded modules
	HANDLE hVMThread;			// The thread the virtual machine is running on
	DWORD dwVMThreadID;			// The ID of the thread the virtual machine is running on
	
	DWORD dwPadding;			// 4-byte padding
#else
#endif
	size_t libraryCount;		// The maximum number of libraries which can be loaded

	vm_flags_t flags;			// Virtual machine startup flags

	ls_stdio_t stdio;			// Standard IO functions
};

struct env_s
{
	byte_t *rip;				// Program counter
	byte_t *cmdStart;			// The start of the current command being executed
	vm_t *vm;					// The virtual machine this environment is apart of

	byte_t *stack;				// Pointer to the start of the block allocated for the stack
	byte_t *rsp;				// Stack pointer - points to the top of the stack
	byte_t *rbp;				// Stack base pointer - points to the current stack frame

	list_t *variables;			// List of maps which map strings to values in scope (stored as a value_t *)

	byte_t cmdHistory[HISTLEN];	// An array of the previous commands executed - updated if launched with -verbose

	int exception;				// The most recent exception which was thrown
	char message[EMSGLEN];		// The message associated with the exception

	union
	{
		byte_t bret;			// 1-byte integer return value
		word_t wret;			// 2-byte integer return value
		dword_t dret;			// 4-byte integer return value
		qword_t qret;			// 8-byte integer return value
		real4_t r4ret;			// 32-bit IEEE 754 floating point return value
		real8_t r8ret;			// 64-bit IEEE 754 floating point return value
		void *vret;				// Object return value
	};
};

/*
Stores information about the virtual machine at a certain time period
*/
struct vm_snapshot_s
{
	qword_t time;	// The time at which this snapshot was taken, in milliseconds

	vm_t *handle;	// The VM's original handle (volatile members)
	list_t *envs;	// A list of env_snapshot_t's for each active environment
	vm_t saved;		// Saved state of the VM

};

struct env_snapshot_s
{
	qword_t time;	// The time at which this snapshot was taken, in milliseconds

	struct
	{
		vm_t *handle;				// The VM's original handle (volatile members)
	} vm;

	// Information about the current execution environment
	struct
	{
		env_t *handle;				// The execution enviornment's handle (volatile members)
		env_t saved;				// Saved state of the execution environment

		byte_t *rsp;				// RSP relative to the copied stack - NULL if copy failed
		byte_t *rbp;				// RBP relative to the copied stack - NULL if copy failed
	} env;

	// Information about the currently executing function
	struct
	{
		function_t *handle;			// The function handle
		size_t relativeLocation;	// The function's location relative to the class
	} function;

	// Information about the execution location
	struct
	{
		size_t execFuncOffset;		// Execution location relative to the function's location
	} exec;
};

/*
Creates a new virtual machine with the designated parameters.

@param heapSize The size of the heap.
@param stackSize The size of the stack, per thread.
@param flags Creation flags.
@param pathCount The length of paths.
@param paths The paths for which the virtual machine will search for classes on. The first
element should be the location of lscript API files (i.e. directory where core classes are
stored such as Class, Object, String, etc.). Although, this is not necessary.
@param stdio A pointer to a ls_stdio_t struct which contains functions which will read or
write from standard streams. If NULL, the default IO functions will be used. If non-NULL,
each NULL field will cause the default IO function to be used for that field.

@return The new virtual machine, or NULL if creation failed.
*/
vm_t *vm_create(size_t heapSize, size_t stackSize, void *lsAPILib, vm_flags_t flags, int pathCount, const char *const paths[], const ls_stdio_t *stdio);

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
class_t *vm_get_class(vm_t *__restrict vm, const char *__restrict classname);

/*
Loads a class with the requested classname if it is not loaded already. The virtual machine
will search on the classpath for a class of the requested name.

@param vm The virtual machine to load the class into.
@param classname The name of the class to load.

@return The respective class, if it already is loaded or was loaded successfully, or NULL
if the load failed.
*/
class_t *vm_load_class(vm_t *__restrict vm, const char *__restrict classname);

/*
Loads a class from a file onto the virtual machine.

@param vm The virtual machine to load the class into.
@param filename The filepath of the class.
@param loadSuperclasses Whether to recursively load any superclasses.

@return The loaded class, or NULL if the load failed.
*/
class_t *vm_load_class_file(vm_t *__restrict vm, const char *__restrict filename, int loadSuperclasses);

/*
Loads a class from binary data onto the virtual machine.

@param vm The virtual machine to load the class into.
@param binary A pointer to the binary data of the class.
@param size The size of the binary data, in bytes.
@param loadSuperclasses Whether to recursively load any superclasses.

@return The loaded class, or NULL if the load failed.
*/
class_t *vm_load_class_binary(vm_t *vm, byte_t *binary, size_t size, int loadSuperclasses);

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
Creates a snapshot of the current execution status of an environment and virtual
machine. Snapshots can be expensive, especially with large heap and stack sizes as
all these are copied. However, even if these data can not be copied, other data
will still be copied.

@param env The environment to take a snapshot of.

@return The snapshot, or NULL if it couldn't be taken.
*/
vm_snapshot_t *vm_take_snapshot(vm_t *vm);

/*
Frees a snapshot taken with vm_take_snapshot.

@param ss The snapshot to free.
*/
void vm_free_snapshot(vm_snapshot_t *ss);

/*
Creates a new execution environment in the specified virtual machine.

@return The new enviornment, or NULL if creation failed.
*/
env_t *env_create(vm_t *vm);

/*
Creates a snapshot of the current execution status of an environment and virtual
machine. Snapshots can be expensive, especially with large heap and stack sizes as
all these are copied. However, even if these data can not be copied, other data
will still be copied.

@param env The environment to take a snapshot of.

@return The snapshot, or NULL if it couldn't be taken.
*/
env_snapshot_t *env_take_snapshot(env_t *env);

/*
Frees a snapshot taken with env_take_snapshot.

@param ss The snapshot to save.
*/
void env_free_snapshot(env_snapshot_t *ss);

/*
Resolves a variable name in the current scope. If the find fails, an exception will be
raised in the environment.

@param env The environment to resolve the variable in.
@param name The name of the variable.
@param data A pointer to a pointer which will point to the data stored in the variable on success.
@param flags A pointer to a flags_t which will store the flags carried by the variable on success.

@return 1 on success and 0 otherwise.
*/
int env_resolve_variable(env_t *env, char *name, data_t **data, flags_t *flags);

/*
Resolves an object field in an environment. If the find fails, an exception will be
raised in the environment.

@param env The environment to resolve the field in.
@param object The source object.
@param data A pointer to a pointer which will point to the data stored in the field on success.
@param flags A pointer to a flags_t which will store the flags carried by the field on success.

@return 1 on success and 0 otherwise.
*/
int env_resolve_object_field(env_t *env, object_t *object, char *name, data_t **data, flags_t *flags);

/*
Resolves a function name in the current scope. If the resolution fails, an exception will
be reaised in the environment.

@param env The environment to resolve the function in.
@param name The name of the function to resolve. This can have local variables and class names preceding
the actual function name, delimited by '.'s.
@param function A pointer to a pointer which will point to the respective function on success.

@return 1 on success and 0 otherwise.
*/
int env_resolve_function_name(env_t *env, char *name, function_t **function);

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
int env_resolve_dynamic_function_name(env_t *env, char *name, function_t **function, data_t **data, flags_t *flags);

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