#if !defined(VM_H)
#define VM_H

#include "mem.h"
#include <stdarg.h>

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
	exception_bad_array_index
};

struct vm_s
{
	list_t *envs;
	map_t *classes;
	manager_t *manager;
	size_t stackSize;
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
		unsigned char bret;
		unsigned short wret;
		unsigned int lret;
		unsigned long long qret;
		void *vret;
	};
};

vm_t *vm_create(size_t heapSize, size_t stackSize);
class_t *vm_get_class(vm_t *vm, const char *classname);
class_t *vm_load_class(vm_t *vm, const char *filename);
class_t *vm_load_class_binary(vm_t *vm, const char *binary, size_t size);
void vm_free(vm_t *vm);

env_t *env_create(vm_t *vm);
int env_run_func_staticv(env_t *env, function_t *function, va_list ls);
int env_run_funcv(env_t *env, function_t *function, object_t *object, va_list ls);

inline int env_run_func_static(env_t *env, function_t *function, ...)
{
	va_list ls;
	va_start(ls, function);
	int result = env_run_func_staticv(env, function, ls);
	va_end(ls);
	return result;
}

inline int env_fun_func(env_t *env, function_t *function, object_t *object, ...)
{
	va_list ls;
	va_start(ls, object);
	int result = env_run_funcv(env, function, object, ls);
	va_end(ls);
	return result;
}

void env_free(env_t *env);


#endif