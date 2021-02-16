#include "../lscript.h"

#include "vm.h"
#include "string_util.h"

#define MAX_PATHS 16

typedef struct vm_args_s vm_args_t;
struct vm_args_s
{
	size_t heapSize;
	size_t stackSize;
	const char *const *argv;
	int argc;
	const char *paths[MAX_PATHS];
};

static int parse_arguments(int argc, const char *const argv[], vm_args_t *argStruct);

static LVM gCurrentVM = NULL;

LEXPORT LVM LCALL ls_create_vm(int argc, const char *const argv[])
{
	vm_args_t args;
	if (gCurrentVM || !parse_arguments(argc, argv, &args))
		return NULL;
	vm_t *vm = vm_create(args.heapSize, args.stackSize, 0, MAX_PATHS, args.paths, args.argc, args.argv);
	return gCurrentVM = vm;
}

LEXPORT LVM LCALL ls_create_and_start_vm(int argc, const char *const argv[], void **threadHandle, unsigned long *threadID)
{
	vm_args_t args;
	if (gCurrentVM || !parse_arguments(argc, argv, &args))
		return NULL;
	vm_t *vm = vm_create(args.heapSize, args.stackSize, 1, MAX_PATHS, args.paths, args.argc, args.argv);
#if defined(_WIN32)
	if (threadHandle)
		*threadHandle = vm->hVMThread;
	if (threadID)
		*threadID = vm->dwVMThreadID;
#else
#endif
	return gCurrentVM = vm;
}

LEXPORT LVM LCALL ls_get_current_vm()
{
	return gCurrentVM;
}

LEXPORT lvoid LCALL ls_destroy_vm(unsigned long threadWaitTime)
{
	if (gCurrentVM)
	{
		vm_free((vm_t *)gCurrentVM, threadWaitTime);
		gCurrentVM = NULL;
	}
}

LEXPORT lvoid LCALL ls_add_to_classpath(const char *path)
{
	vm_add_path(gCurrentVM, path);
}

LEXPORT lclass LCALL ls_load_class_file(const char *filepath)
{
	return vm_load_class_file(gCurrentVM, filepath);
}

LEXPORT lclass LCALL ls_load_class_data(unsigned char *data, luint datalen)
{
	return vm_load_class_binary(gCurrentVM, data, datalen);
}

LEXPORT lclass LCALL ls_load_class_name(const lchar *classname)
{
	return vm_load_class(gCurrentVM, classname);
}

LEXPORT lclass LCALL ls_class_for_name(const lchar *classname)
{
	return vm_get_class(gCurrentVM, classname);
}

LEXPORT lfield LCALL ls_get_field(lclass clazz, const lchar *name)
{
	lfield result = NULL;
	result = class_get_dynamic_field_offset((class_t *)clazz, name);
	if (!result)
		result = class_get_static_field((class_t *)clazz, name);
	return result;
}

LEXPORT lfunction LCALL ls_get_function(lclass clazz, const lchar *qualifiedName)
{
	return (lfunction)class_get_function(clazz, qualifiedName);
}

LEXPORT lvoid LCALL ls_call_void_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_funcv((env_t *)env, (function_t *)function, (object_t *)object, list);
}

LEXPORT lvoid LCALL ls_call_static_void_functionv(LEnv env, lfunction function, va_list list)
{
	env_run_func_staticv((env_t *)env, (function_t *)function, list);
}

LEXPORT luint LCALL ls_get_array_length(lobject array)
{
	return ls_get_array_length(array);
}

int parse_arguments(int argc, const char *const argv[], vm_args_t *argStruct)
{
	int pathInd = 0;
	memset(argStruct, 0, sizeof(vm_args_t));
	argStruct->heapSize = DEFAULT_HEAP_SIZE;
	argStruct->stackSize = DEFAULT_STACK_SIZE;
	for (int i = 0; i < argc; i++)
	{
		if (equals_ignore_case("-v", argv[i]))
		{

		}
		else if (equals_ignore_case("-h", argv[i]))
		{

		}
		else if (equals_ignore_case("-path", argv[i]))
		{
			i++;
			if (i < argc)
			{
				argStruct->paths[pathInd++] = argv[i];
			}
		}
		else
		{
			argStruct->argc = argc - i;
			argStruct->argv = &argv[i];
			break;
		}
	}
	return 1;
}
