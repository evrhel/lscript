#include "../lscript.h"

#include "vm.h"
#include "string_util.h"

#define MAX_PATHS 16

typedef struct vm_args_s vm_args_t;
struct vm_args_s
{
	size_t heapSize;
	size_t stackSize;
	vm_flags_t flags;
	const char *const *argv;
	int argc;
	const char *paths[MAX_PATHS];
};

static int parse_arguments(int argc, const char *const argv[], vm_args_t *argStruct);
static void print_help();

static vm_t *gCurrentVM = NULL;

LEXPORT LVM LCALL ls_create_vm(int argc, const char *const argv[], void *lsAPILib)
{
	vm_args_t args;
	if (gCurrentVM || !parse_arguments(argc, argv, &args))
		return NULL;
	vm_t *vm = vm_create(args.heapSize, args.stackSize, lsAPILib, args.flags, MAX_PATHS, args.paths);
	return gCurrentVM = vm;
}

LEXPORT lint LCALL ls_start_vm(int argc, const char *const argv[], void **threadHandle, unsigned long *threadID)
{
	int result;
	
	result = vm_start(gCurrentVM, threadHandle != NULL, argc, argv);
	if (threadHandle)
		*threadHandle = gCurrentVM->hVMThread;
	if (threadID)
		*threadID = gCurrentVM->dwVMThreadID;
	return result;
}

LEXPORT LVM LCALL ls_create_and_start_vm(int argc, const char *const argv[], void **threadHandle, unsigned long *threadID, void *lsAPILib)
{
	vm_args_t args;
	if (gCurrentVM || !parse_arguments(argc, argv, &args))
		return NULL;
	vm_t *vm = vm_create(args.heapSize, args.stackSize, lsAPILib, args.flags, MAX_PATHS, args.paths);
	gCurrentVM = vm;
	if (!gCurrentVM)
		return NULL;
	if (!ls_start_vm(args.argc, args.argv, threadHandle, threadID))
		return NULL;
	return gCurrentVM;
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

LEXPORT lchar LCALL ls_call_char_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_funcv((env_t *)env, (function_t *)function, (object_t *)object, list);
	return (lchar)((env_t *)env)->bret;
}

LEXPORT luchar LCALL ls_call_uchar_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_funcv((env_t *)env, (function_t *)function, (object_t *)object, list);
	return (luchar)((env_t *)env)->bret;
}

LEXPORT lshort LCALL ls_call_short_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_funcv((env_t *)env, (function_t *)function, (object_t *)object, list);
	return (lshort)((env_t *)env)->wret;
}

LEXPORT lushort LCALL ls_call_ushort_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_funcv((env_t *)env, (function_t *)function, (object_t *)object, list);
	return (lushort)((env_t *)env)->wret;
}

LEXPORT lint LCALL ls_call_int_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_funcv((env_t *)env, (function_t *)function, (object_t *)object, list);
	return (lint)((env_t *)env)->dret;
}

LEXPORT luint LCALL ls_call_uint_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_funcv((env_t *)env, (function_t *)function, (object_t *)object, list);
	return (luint)((env_t *)env)->dret;
}

LEXPORT llong LCALL ls_call_long_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_funcv((env_t *)env, (function_t *)function, (object_t *)object, list);
	return (llong)((env_t *)env)->qret;
}

LEXPORT lulong LCALL ls_call_ulong_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_funcv((env_t *)env, (function_t *)function, (object_t *)object, list);
	return (lulong)((env_t *)env)->qret;
}

LEXPORT lbool LCALL ls_call_bool_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_funcv((env_t *)env, (function_t *)function, (object_t *)object, list);
	return (lbool)((env_t *)env)->bret;
}

LEXPORT lfloat LCALL ls_call_float_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_funcv((env_t *)env, (function_t *)function, (object_t *)object, list);
	return (lfloat)((env_t *)env)->r4ret;
}

LEXPORT ldouble LCALL ls_call_double_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_funcv((env_t *)env, (function_t *)function, (object_t *)object, list);
	return (ldouble)((env_t *)env)->r8ret;
}

LEXPORT lobject LCALL ls_call_object_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_funcv((env_t *)env, (function_t *)function, (object_t *)object, list);
	return (lobject)((env_t *)env)->vret;
}

LEXPORT lvoid LCALL ls_call_static_void_functionv(LEnv env, lfunction function, va_list list)
{
	env_run_func_staticv((env_t *)env, (function_t *)function, list);
}

LEXPORT lchar LCALL ls_call_static_char_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_func_staticv((env_t *)env, (function_t *)function, list);
	return (lchar)((env_t *)env)->bret;
}

LEXPORT luchar LCALL ls_call_static_uchar_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_func_staticv((env_t *)env, (function_t *)function, list);
	return (luchar)((env_t *)env)->bret;
}

LEXPORT lshort LCALL ls_call_static_short_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_func_staticv((env_t *)env, (function_t *)function, list);
	return (lshort)((env_t *)env)->wret;
}

LEXPORT lushort LCALL ls_call_static_ushort_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_func_staticv((env_t *)env, (function_t *)function, list);
	return (lushort)((env_t *)env)->wret;
}

LEXPORT lint LCALL ls_call_static_int_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_func_staticv((env_t *)env, (function_t *)function, list);
	return (lint)((env_t *)env)->dret;
}

LEXPORT luint LCALL ls_call_static_uint_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_func_staticv((env_t *)env, (function_t *)function, list);
	return (luint)((env_t *)env)->dret;
}

LEXPORT llong LCALL ls_call_static_long_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_func_staticv((env_t *)env, (function_t *)function, list);
	return (llong)((env_t *)env)->qret;
}

LEXPORT lulong LCALL ls_call_static_ulong_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_func_staticv((env_t *)env, (function_t *)function, list);
	return (lulong)((env_t *)env)->qret;
}

LEXPORT lbool LCALL ls_call_static_bool_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_func_staticv((env_t *)env, (function_t *)function, list);
	return (lbool)((env_t *)env)->bret;
}

LEXPORT lfloat LCALL ls_call_static_float_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_func_staticv((env_t *)env, (function_t *)function, list);
	return (lfloat)((env_t *)env)->r4ret;
}

LEXPORT ldouble LCALL ls_call_static_double_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_func_staticv((env_t *)env, (function_t *)function, list);
	return (lfloat)((env_t *)env)->r8ret;
}

LEXPORT lobject LCALL ls_call_static_object_functionv(LEnv env, lfunction function, lobject object, va_list list)
{
	env_run_func_staticv((env_t *)env, (function_t *)function, list);
	return (lobject)((env_t *)env)->vret;
}

LEXPORT luint LCALL ls_get_array_length(lobject array)
{
	return ((array_t *)array)->length;
}

int parse_arguments(int argc, const char *const argv[], vm_args_t *argStruct)
{
	int pathInd = 0;
	memset(argStruct, 0, sizeof(vm_args_t));
	argStruct->heapSize = DEFAULT_HEAP_SIZE;
	argStruct->stackSize = DEFAULT_STACK_SIZE;
	for (int i = 0; i < argc; i++)
	{
		if (equals_ignore_case("-version", argv[i]))
		{
			printf("lscript version \"%s\"\n", LS_VERSION);
			printf("build date: %s\n", __DATE__);
			printf("build time: %s\n", __TIME__);
		}
		else if (equals_ignore_case("-help", argv[i]) || equals_ignore_case("-?", argv[i]))
		{
			print_help();
			return 0;
		}
		else if (equals_ignore_case("-verbose", argv[i]))
		{
			argStruct->flags |= vm_flag_verbose;
		}
		else if (equals_ignore_case("-nodebug", argv[i]))
		{
			argStruct->flags |= vm_flag_no_load_debug;
		}
		else if (equals_ignore_case("-verr", argv[i]))
		{
			argStruct->flags |= vm_flag_verbose_errors;
		}
		else if (equals_ignore_case("-path", argv[i]))
		{
			i++;
			if (i < argc)
			{
				argStruct->paths[pathInd++] = argv[i];
			}
			else
			{
				print_help();
				return 0;
			}
		}
		else if (equals_ignore_case("-heapsize", argv[i]))
		{
			i++;
			if (i < argc)
			{

			}
			else
			{
				print_help();
				return 0;
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

void print_help()
{
	printf("Usage: lscript [-options] <class> [args...]\n");
	printf("       (Executes class <class> on the classpath with [args...]\n");
	printf("       passed to its main function)\n");
	printf("Where options include:\n");
	printf("  -version      Displays version information and exists.\n");
	printf("  -help -?      Prints this help message.\n");
	printf("  -verbose      Enable verbose output.\n");
	printf("  -nodebug      Disables loading of debugging symbols.\n");
	printf("  -verr         Enables only verbose error output. Has no effect if\n");
	printf("                -verbose is specified.\n");
	printf("  -path <path>  Adds <path> to the claspath.\n");
}
