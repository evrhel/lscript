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

typedef struct vm_s vm_t;
typedef struct env_s env_t;

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

struct vm_s
{
	list_t *envs;
	map_t *classes;
	manager_t *manager;
	size_t stackSize;
	map_t *properties;

	list_t *paths;

#if defined(WIN32)
	HMODULE *hLibraries;
#else
#endif
	size_t libraryCount;
};

struct env_s
{
	void *rip;
	vm_t *vm;

	void *stack;
	void *rsp, *rbp;

	list_t *variables;

	int exception, exceptionDesc;

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

vm_t *vm_create(size_t heapSize, size_t stackSize, int argc, const char *const argv[]);
class_t *vm_get_class(vm_t *vm, const char *classname);
class_t *vm_load_class(vm_t *vm, const char *classname);
class_t *vm_load_class_file(vm_t *vm, const char *filename);
class_t *vm_load_class_binary(vm_t *vm, byte_t *binary, size_t size);
void vm_add_path(vm_t *vm, const char *path);
int vm_load_library(vm_t *vm, const char *libpath);
void vm_free(vm_t *vm);

env_t *env_create(vm_t *vm);
int env_resolve_variable(env_t *env, const char *name, data_t **data, flags_t *flags);
int env_resolve_object_field(env_t *env, object_t *object, const char *name, data_t **data, flags_t *flags);
int env_resolve_function_name(env_t *env, const char *name, function_t **function);
int env_resolve_dynamic_function_name(env_t *env, const char *name, function_t **function, data_t **data, flags_t *flags);
int env_run_func_staticv(env_t *env, function_t *function, va_list ls);
int env_run_funcv(env_t *env, function_t *function, object_t *object, va_list ls);
object_t *env_new_string(env_t *env, const char *cstring);

inline int env_run_func_static(env_t *env, function_t *function, ...)
{
	va_list ls;
	va_start(ls, function);
	int result = env_run_func_staticv(env, function, ls);
	va_end(ls);
	return result;
}

inline int env_run_func(env_t *env, function_t *function, object_t *object, ...)
{
	va_list ls;
	va_start(ls, object);
	int result = env_run_funcv(env, function, object, ls);
	va_end(ls);
	return result;
}

void env_free(env_t *env);


#endif