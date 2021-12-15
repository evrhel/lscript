#include "vm.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mem_debug.h"
#include "cast.h"
#include "value.h"
#include "vm_math.h"
#include "vm_compare.h"
#include "string_util.h"
#include "debug.h"
#include "lprocess.h"

#define WORD_SIZE sizeof(size_t)

//#define CURR_CLASS(env) (*(((class_t**)(env)->rbp)+2))
#define CURR_FLAGS(env) (*(((frame_flags_t*)(env)->rbp)-1))
#define CURR_FUNC(env) (*(((function_t**)(env)->rbp)-2))
#define FRAME_FUNC(rbp) (*(((function_t**)(rbp))-2))
#define FRAME_RIP(rbp) (*(((byte_t**)(rbp))-3))
#define PREV_FRAME(rbp) (*(((byte_t**)(rbp))-4))

#define EXIT_RUN(val) {__retVal=(val);goto done_call;}

#define CLEAR_EXCEPTION(env) { env->exception = 0; env->message[0] = 0; }

typedef unsigned long long frame_flags_t;

static const char *const g_exceptionStrings[] =
{
	"NO_EXCEPTION",
	"OUT_OF_MEMORY",
	"STACK_OVERFLOW",
	"BAD_COMMAND",
	"VM_ERROR",
	"ILLEGAL_STATE",
	"CLASS_NOT_FOUND",
	"FUNCTION_NOT_FOUND",
	"FIELD_NOT_FOUND",
	"NULL_DEREFERENCE",
	"BAD_VARIABLE_NAME",
	"BAD_ARRAY_INDEX",
	"LINK_ERROR"
};

typedef struct start_args_s start_args_t;
struct start_args_s
{
	vm_t *vm;
	array_t *args;
};

enum
{
	frame_flag_return_no_cleanup = 0x1,
	frame_flag_return_native = 0x2,
};

static int class_resolve_filename(vm_t *__restrict vm, const char *__restrict classname, char *filename, size_t filenameLen);
static class_t *class_load_to_vm(vm_t *__restrict vm, class_t *__restrict clazz);
static class_t *class_load_filename_override(vm_t *__restrict vm, const char *__restrict classname, const char *__restrict filename, int loadSuperClasses, int loadToVM, int checkExists);
static char *resolve_first_class(vm_t *vm, char *name, class_t **result);

static class_t *class_load_ext(const char *classname, vm_t *vm);

static int env_run(env_t *__restrict env, void *__restrict location);
static inline int env_create_stack_frame(env_t *__restrict env, function_t *__restrict function, flags_t flags);
static inline int env_cleanup_call(env_t *__restrict env, int onlyStackCleanup);

static int env_handle_static_function_callv(env_t *__restrict env, function_t *__restrict function, frame_flags_t flags, va_list ls);
static int env_handle_dynamic_function_callv(env_t *__restrict env, function_t *__restrict function, frame_flags_t flags, object_t *object, va_list ls);

static va_list env_gen_call_arg_list(env_t *env, function_t *function);
static inline void env_free_call_arg_list(env_t *env, va_list callArgs);

static void *stack_push(env_t *env, value_t *value);
static void *stack_alloc(env_t *env, size_t words);
static int stack_pop(env_t *env, size_t words, qword_t *dstWords);

static int is_varname_avaliable(env_t *env, const char *name);

static int static_set(data_t *dst, flags_t dstFlags, data_t *src, flags_t srcFlags);

static int try_link_function(vm_t *__restrict vm, function_t *__restrict func);

static void vm_start_routine(start_args_t *args);

static void print_stack_trace(FILE *file, env_t *env, int printVars);

static size_t default_write_stdout(const char *buf, size_t count);
static size_t default_write_stderr(const char *buf, size_t count);
static size_t default_read_stdin(char *buf, size_t size, size_t count);
static char default_read_char_stdin();

/*
Calls a function with the desired arguments.

Implemented in hooks.asm

@param argCount The number of arguments the function takes.
@param argTypes The type of each respective argument. Unused internally - pass as NULL.
@param args A pointer to the start of the arguments. Each argument should be 8-bytes wide.
@param proc The function to execute.

@return The return value.
*/
extern qword_t __cdecl vm_call_extern_asm(size_t argCount, const byte_t *argTypes, const void *args, void *proc);

static inline void store_return(env_t *env, data_t *dst, flags_t dstFlags)
{
	byte_t type = value_typeof((value_t *)&dstFlags);
	switch (type)
	{
	case lb_char:
	case lb_uchar:
	case lb_bool:
		dst->ucvalue = env->bret;
		break;
	case lb_short:
	case lb_ushort:
		dst->usvalue = env->wret;
		break;
	case lb_int:
	case lb_uint:
	case lb_float:
		dst->uivalue = env->dret;
		break;
	case lb_long:
	case lb_ulong:
	case lb_double:
	case lb_object:
	case lb_chararray:
	case lb_uchararray:
	case lb_shortarray:
	case lb_ushortarray:
	case lb_intarray:
	case lb_uintarray:
	case lb_longarray:
	case lb_ulongarray:
	case lb_boolarray:
	case lb_floatarray:
	case lb_doublearray:
	case lb_objectarray:
		dst->ulvalue = env->qret;
		break;
	}
}

static inline int is_numeric(const char *string)
{
	while (*string)
	{
		if (*string > '9' || *string < '0')
			return 0;
		string++;
	}
	return 1;
}

static inline int handle_if(env_t *env)
{
	if (!vmc_compare(env, &env->rip))
	{
		if (env->exception)
			return env->exception;
		class_t *c = CURR_FUNC(env)->parentClass;
		assert(*((size_t *)(env->rip)) != (size_t)-1);
		env->rip = c->data + *((size_t *)(env->rip));
		//env->rip += *((size_t *)(env->rip));
		//env->rip += sizeof(size_t);
	}
	else
		env->rip += sizeof(size_t);
	return 0;
}

vm_t *vm_create(size_t heapSize, size_t stackSize, void *lsAPILib, vm_flags_t flags, int pathCount, const char *const paths[], const ls_stdio_t *stdio)
{
	vm_t *vm = (vm_t *)MALLOC(sizeof(vm_t));
	if (!vm)
		return NULL;

	if (stdio)
	{
		vm->stdio.writeStdoutFunc = stdio->writeStdoutFunc == DEFAULT_WRITE_STDOUT ? default_write_stdout : stdio->writeStdoutFunc;
		vm->stdio.writeStderrFunc = stdio->writeStderrFunc == DEFAULT_WRITE_STDERR ? default_write_stderr : stdio->writeStderrFunc;
		vm->stdio.readStdinFunc = stdio->readStdinFunc == DEFAULT_READ_STDIN ? default_read_stdin : stdio->readStdinFunc;
		vm->stdio.readCharStdinFunc = stdio->readCharStdinFunc == DEFAULT_READ_CHAR_STDIN ? default_read_char_stdin : stdio->readCharStdinFunc;
	}
	else
	{
		vm->stdio.writeStdoutFunc = default_write_stdout;
		vm->stdio.writeStderrFunc = default_write_stderr;
		vm->stdio.readStdinFunc = default_read_stdin;
		vm->stdio.readCharStdinFunc = default_read_char_stdin;
	}

	vm->classes = map_create(16, string_hash_func, string_compare_func, string_copy_func, NULL, (free_func_t)free);
	if (!vm->classes)
	{
		FREE(vm);
		return NULL;
	}

	vm->envs = NULL;
	vm->envsLast = vm->envs;

	vm->manager = manager_create(heapSize);
	if (!vm->manager)
	{
		list_free(vm->envs, 0);
		map_free(vm->classes, 0);
		FREE(vm);
		return NULL;
	}

	vm->stackSize = stackSize;

	vm->paths = list_create();
	if (!vm->paths)
	{
		manager_free(vm->manager);
		list_free(vm->envs, 0);
		map_free(vm->classes, 0);
		FREE(vm);
		return NULL;
	}
	char current[] = ".\\";
	vm->paths->data = MALLOC(sizeof(current));
	if (!vm->paths->data)
	{
		list_free(vm->paths, 0);
		manager_free(vm->manager);
		list_free(vm->envs, 0);
		map_free(vm->classes, 0);
		FREE(vm);
		return NULL;
	}
	MEMCPY(vm->paths->data, current, sizeof(current));

	for (int i = 0; i < pathCount; i++)
	{
		if (paths[i])
			vm_add_path(vm, paths[i]);
	}

	vm->loadedClassObjects = map_create(16, string_hash_func, string_compare_func, string_copy_func, NULL, (free_func_t)free);

	vm->libraryCount = 4;
#if defined(_WIN32)
	vm->hLibraries = (HMODULE *)CALLOC(vm->libraryCount, sizeof(HMODULE));
	if (!vm->hLibraries)
	{
		list_free(vm->paths, 1);
		manager_free(vm->manager);
		list_free(vm->envs, 0);
		map_free(vm->classes, 0);
		FREE(vm);
		return NULL;
	}
	vm->hLibraries[0] = (HMODULE)lsAPILib;//GetModuleHandleA(NULL);
	vm->hVMThread = NULL;
	vm->dwVMThreadID = 0;
#else
#endif

	vm->flags = flags;
	
	// Load all the required classes

	char classFilename[MAX_PATH];

	static const char OBJECT_CLASS[] = "lscript.lang.Object";
	static const char CLASS_CLASS[] = "lscript.lang.Class";
	static const char STRING_CLASS[] = "lscript.lang.String";

	// Load Object class
	if (!class_resolve_filename(vm, OBJECT_CLASS, classFilename, sizeof(classFilename)))
	{
		vm_free(vm, 0);
		return NULL;
	}

	class_t *objectClass = class_load_filename_override(vm, OBJECT_CLASS, classFilename, 0, 0, 0);
	if (!objectClass)
	{
		vm_free(vm, 0);
		return NULL;
	}

	// Load Class class
	if (!class_resolve_filename(vm, CLASS_CLASS, classFilename, sizeof(classFilename)))
	{
		vm_free(vm, 0);
		return NULL;
	}

	class_t *classClass = class_load_filename_override(vm, CLASS_CLASS, classFilename, 0, 0, 0);
	if (!classClass)
	{
		vm_free(vm, 0);
		return NULL;
	}

	// Load String class
	if (!class_resolve_filename(vm, STRING_CLASS, classFilename, sizeof(classFilename)))
	{
		vm_free(vm, 0);
		return NULL;
	}

	class_t *stringClass = class_load_filename_override(vm, STRING_CLASS, classFilename, 0, 0, 0);
	if (!stringClass)
	{
		vm_free(vm, 0);
		return NULL;
	}

	set_superclass(classClass, objectClass);
	set_superclass(stringClass, objectClass);

	class_load_to_vm(vm, objectClass);
	class_load_to_vm(vm, classClass);
	class_load_to_vm(vm, stringClass);

	// Load System class
	class_t *systemClass = vm_load_class(vm, "lscript.lang.System");
	if (!systemClass)
	{
		vm_free(vm, 0);
		return NULL;
	}

	// Load StdFileHandle class
	class_t *stdFileHandleClass = vm_load_class(vm, "lscript.io.StdFileHandle");
	if (!stdFileHandleClass)
	{
		vm_free(vm, 0);
		return NULL;
	}

	// Load FileOutputStream class
	class_t *fileoutputstreamClass = vm_load_class(vm, "lscript.io.FileOutputStream");
	if (!fileoutputstreamClass)
	{
		vm_free(vm, 0);
		return NULL;
	}

	// Load FileInputStream class
	class_t *fileinputstreamClass = vm_load_class(vm, "lscript.io.FileInputStream");
	if (!fileinputstreamClass)
	{
		vm_free(vm, 0);
		return NULL;
	}

	// Get the stdout static field from System
	value_t *systemStdout = class_get_static_field(systemClass, "stdout");
	if (!systemStdout)
	{
		vm_free(vm, 0);
		return NULL;
	}

	// Get the stderr static field from System
	value_t *systemStderr = class_get_static_field(systemClass, "stderr");
	if (!systemStderr)
	{
		vm_free(vm, 0);
		return NULL;
	}

	// Get the stdin static field from System
	value_t *systemStdin = class_get_static_field(systemClass, "stdin");
	if (!systemStdin)
	{
		vm_free(vm, 0);
		return NULL;
	}

	// Allocate a FileOutputStream object for System.stdout
	object_t *stdoutVal = manager_alloc_object(vm->manager, fileoutputstreamClass);
	if (!stdoutVal)
	{
		vm_free(vm, 0);
		return NULL;
	}
	systemStdout->ovalue = stdoutVal;

	// Allocate a FileOutputStream object for System.stderr
	object_t *stderrVal = manager_alloc_object(vm->manager, fileoutputstreamClass);
	if (!stderrVal)
	{
		vm_free(vm, 0);
		return NULL;
	}
	systemStderr->ovalue = stderrVal;

	// Allocate a FileInputStream object for System.stdin
	object_t *stdinVal = manager_alloc_object(vm->manager, fileinputstreamClass);
	if (!stdinVal)
	{
		vm_free(vm, 0);
		return NULL;
	}
	systemStdin->ovalue = stdinVal;

	object_t *stdHandles[3] =
	{
		manager_alloc_object(vm->manager, stdFileHandleClass),	// stdout
		manager_alloc_object(vm->manager, stdFileHandleClass),	// stderr
		manager_alloc_object(vm->manager, stdFileHandleClass)	// stdin
	};

	// Set each nativeHandle field to the FILE pointer for each stream
	object_set_ulong(stdHandles[0], "nativeHandle", (lulong)stdout);
	object_set_ulong(stdHandles[1], "nativeHandle", (lulong)stderr);
	object_set_ulong(stdHandles[2], "nativeHandle", (lulong)stdin);

	object_set_object(stdoutVal, "handle", stdHandles[0]);
	object_set_object(stderrVal, "handle", stdHandles[1]);
	object_set_object(stdinVal, "handle", stdHandles[2]);

#if defined(_WIN32)
	vm->hVMThread = NULL;
	vm->dwVMThreadID = 0;
#else
#endif

	return vm;
}

int vm_start(vm_t *vm, int startOnNewThread, int argc, const char *const argv[])
{
#if defined(_WIN32)
	if (startOnNewThread)
	{
		if (argc == 0)
			return 0;

		env_t *tempEnv = env_create(vm);
		array_t *stringArgs = env_new_string_array(tempEnv, argc - 1, argv + 1);
		env_free(tempEnv);

		if (!vm_load_class(vm, argv[0]))
		{
			vm_free(vm, 0);
			return 0;
		}

		start_args_t *start = (start_args_t *)MALLOC(sizeof(start_args_t));
		if (!start)
		{
			vm_free(vm, 0);
			return 0;
		}
		start->vm = vm;
		start->args = stringArgs;

		vm->hVMThread = CreateThread(
			NULL,
			0,
			(LPTHREAD_START_ROUTINE)vm_start_routine,
			start,
			0,
			&vm->dwVMThreadID
		);

		if (!vm->hVMThread)
		{
			vm_free(vm, 0);
			return 0;
		}
	}
#else
#endif

	return 1;
}

class_t *vm_get_class(vm_t *__restrict vm, const char *__restrict classname)
{
	return (class_t *)map_at(vm->classes, classname);
}

class_t *vm_load_class(vm_t *__restrict vm, const char *__restrict classname)
{
	class_t *result = (class_t *)map_at(vm->classes, classname);
	if (result) return result;

	char filename[MAX_PATH];
	if (!class_resolve_filename(vm, classname, filename, sizeof(filename)))
	{
		if ((vm->flags & vm_flag_verbose) || (vm->flags & vm_flag_verbose_errors))
			printf("Class load error for \"%s\": Failed to locate on classpath.\n", classname);
		return NULL;
	}

	return class_load_filename_override(vm, classname, filename, 1, 1, 0);
}

class_t *vm_load_class_file(vm_t *__restrict vm, const char *__restrict filename, int loadSuperclasses)
{
	FILE *file;

	fopen_s(&file, filename, "rb");
	if (!file)
		return NULL;

	fseek(file, 0, SEEK_END);
	long size = ftell(file);

	fseek(file, 0, SEEK_SET);

	byte_t *binary = (byte_t *)MALLOC(size);
	if (!binary)
	{
		fclose(file);
		return NULL;
	}
	fread_s(binary, size, sizeof(byte_t), size, file);

	fclose(file);

	return vm_load_class_binary(vm, binary, size, loadSuperclasses);
}

class_t *vm_load_class_binary(vm_t *vm, byte_t *binary, size_t size, int loadSuperclasses)
{
	class_t *clazz = class_load(binary, size, loadSuperclasses, (classloadproc_t)class_load_ext, vm);
	if (clazz)
		map_insert(vm->classes, clazz->name, clazz);
	return clazz;
}

object_t *vm_get_class_object(vm_t *vm, const char *classname)
{
	return ((reference_t *)map_at(vm->loadedClassObjects, classname))->object;
}

void vm_add_path(vm_t *vm, const char *path)
{
	list_t *node = vm->paths;
	while (node->next)
		node = node->next;
	size_t pathlen = strlen(path);
	int needPathSeparator = path[pathlen - 1] != '\\';
	size_t size = pathlen + needPathSeparator + 1;
	char *buf = (char *)MALLOC(size);
	if (buf)
	{
		MEMCPY(buf, path, pathlen);
		if (needPathSeparator)
			buf[size - 2] = '\\';
		buf[size - 1] = 0;
		list_insert(node, buf);
	}
}

int vm_load_library(vm_t *vm, const char *libpath)
{
#if defined(_WIN32)
	char buf[MAX_PATH];
	sprintf_s(buf, sizeof(buf), "%s.dll", libpath);
	// Slot 0 is reserved for the lsapi functions
	for (size_t i = 1; i < vm->libraryCount; i++)
	{
		if (!vm->hLibraries[i])
		{
			HMODULE hmodLib;
			hmodLib = LoadLibraryA(buf);
			if (!hmodLib)
				return 0;
			vm->hLibraries[i] = hmodLib;
			return 1;
		}
	}
#else
#endif
	return 0;
}

void vm_free(vm_t *vm, unsigned long threadWaitTime)
{
	cleanup_processes();

#if defined(_WIN32)
	if (vm->hVMThread)
	{
		if (threadWaitTime)
			WaitForSingleObject(vm->hVMThread, threadWaitTime);
		CloseHandle(vm->hVMThread);
	}
#else
#endif

	list_iterator_t *lit = list_create_iterator(vm->envs);
	while (lit)
	{
		if (lit->data)
			env_free((env_t * )lit->data);

		lit = list_iterator_next(lit);
	}

	list_free(vm->envs, 0);


	map_iterator_t *mit = map_create_iterator(vm->classes);

	while (mit->node)
	{
		class_t *clazz = (class_t *)mit->value;
		class_free(clazz, 1);
		mit = map_iterator_next(mit);
	}

	map_free(vm->classes, 0);

	manager_free(vm->manager);

	map_free(vm->loadedClassObjects, 1);

#if defined(_WIN32)
	// Don't free the first library - it is passed in vm_create by user
	for (size_t i = 1; i < vm->libraryCount; i++)
	{
		if (vm->hLibraries[i])
		{
			FreeLibrary(vm->hLibraries[i]);
			vm->hLibraries[i] = NULL;
		}
	}
#else
#endif

	list_free(vm->paths, 1);

	FREE(vm);
}

vm_snapshot_t *vm_take_snapshot(vm_t *vm)
{
	return NULL;
}

void vm_free_snapshot(vm_snapshot_t *ss)
{
}

env_t *env_create(vm_t *vm)
{
	if (vm->flags & vm_flag_verbose)
		printf("Creating new execution environment...");

	env_t *env = (env_t *)MALLOC(sizeof(env_t));
	if (!env)
	{
		if ((vm->flags & vm_flag_verbose) || (vm->flags & vm_flag_verbose_errors))
			printf("\nExecution environment failure: Failed to allocate environment structure.\n");
		return NULL;
	}

	env->stack = MALLOC(vm->stackSize);
	if (!env->stack)
	{
		if ((vm->flags & vm_flag_verbose) || (vm->flags & vm_flag_verbose_errors))
			printf("\nExecution environment failure: Failed to allocate stack.\n");
		FREE(env);
		return NULL;
	}
	env->rsp = env->stack + vm->stackSize;
	env->rbp = env->rsp;

	env->variables = list_create();
	env->variables->data = NULL;


	env->variables->data = (void *)0xdeadcafedeadcafe;

	env->rip = NULL;
	env->vm = vm;
	env->exception = exception_none;
	env->message[0] = 0;
	env->qret = 0;

	value_t val;
	val.flags = 0;
	val.ovalue = (lobject)0xdeadbeefdeadbeef;
	value_set_type(&val, lb_object);
	stack_push(env, &val);

	if (vm->envsLast)
	{
		vm->envsLast->next = list_create();
		vm->envsLast->prev = vm->envsLast;
		vm->envsLast = vm->envsLast->next;
		vm->envsLast->data = env;
	}
	else
	{
		vm->envs = list_create();
		vm->envsLast = vm->envs;
		vm->envsLast->data = env;
	}

	MEMSET(env->cmdHistory, 0, sizeof(env->cmdHistory));

	if (vm->flags & vm_flag_verbose)
		printf(" Done (%p)\n", (void *)env);

	return env;
}

env_snapshot_t *env_take_snapshot(env_t *env)
{
	env_snapshot_t *ss = (env_snapshot_t *)MALLOC(sizeof(env_snapshot_t));
	if (!ss)
		return NULL;

	ss->time = GetTickCount64();

	// Populate VM information
	ss->vm.handle = env->vm;

	// Populate environment information
	ss->env.handle = env;
	memcpy(&ss->env.saved, env, sizeof(env_t));

	ss->env.rsp = NULL;
	ss->env.rbp = NULL;

	env_t *senv = &ss->env.saved;
	senv->stack = (byte_t *)MALLOC(env->vm->stackSize);
	if (senv->stack)
	{
		MEMCPY(senv->stack, env->stack, env->vm->stackSize);

		ss->env.rsp = senv->stack + (env->rsp - env->stack);
		ss->env.rbp = senv->stack + (env->rbp - env->stack);
	}

	ss->env.saved.variables = NULL; // Nothing for right now, would require taking heap snapshot

	// Populate function information
	ss->function.handle = CURR_FUNC(env);
	ss->function.relativeLocation =
		(char *)ss->function.handle->location -
		(char *)ss->function.handle->parentClass->data;

	// Populate execution information
	ss->exec.execFuncOffset =
		(char *)ss->function.handle->location -
		(char *)env->cmdStart;

	return ss;
}

void env_free_snapshot(env_snapshot_t *ss)
{
	if (ss)
	{
		if (ss->env.saved.rsp)
		{
			FREE(ss->env.saved.stack);
			ss->env.saved.stack = NULL;
		}
		FREE(ss);
	}
}

int env_resolve_variable(env_t *env, char *name, data_t **data, flags_t *flags)
{
	map_node_t *mapNode;
	char *beg = strchr(name, '.');
	char *indBeg;
	size_t valsize;

	// If we have a '.', we need to access the variable as an object fetching its field(s)
	if (beg)
	{
		*beg = 0;
		mapNode = map_find((map_t *)env->variables->data, name);

		if (mapNode)
		{
			*beg = '.';
			beg++;
			return env_resolve_object_field(env, (object_t *)((value_t *)mapNode->value)->ovalue, beg, data, flags);
		}
		else
		{
			char classBuild[MAX_PATH];
			class_t *clazz;
			
			*beg = '.';
			beg = resolve_first_class(env->vm, name, &clazz);

			if (!clazz)
			{
				env_raise_exception(env, exception_bad_variable_name, name);
				return 0;
			}

			beg++;

			value_t *fieldVal;

			char *nbeg = strchr(beg, '.');
			if (nbeg)
			{
				*nbeg = 0;
				fieldVal = class_get_static_field(clazz, beg);
				*nbeg = '.';
				if (!fieldVal)
				{
					env_raise_exception(env, exception_bad_variable_name, name);
					return 0;
				}
				nbeg++;
				return env_resolve_object_field(env, (object_t *)((value_t *)fieldVal->ovalue), nbeg, data, flags);
			}
			else
			{
				indBeg = strchr(name, '[');
				if (indBeg)
				{
					*indBeg = 0;
					fieldVal = class_get_static_field(clazz, beg);
					if (!fieldVal)
					{
						env_raise_exception(env, exception_bad_variable_name, name);
						return 0;
					}
					*indBeg = '[';

					char *num = indBeg + 1;
					char *numEnd = strchr(num, ']');
					luint index;
					if (!numEnd)
					{
						env_raise_exception(env, exception_bad_variable_name, name);
						return 0;
					}

					*numEnd = 0;
					if (!is_numeric(num))
					{
						data_t *indexData;
						flags_t indexFlags;
						if (!env_resolve_variable(env, num, &indexData, &indexFlags))
						{
							*numEnd = ']';
							env_raise_exception(env, exception_bad_variable_name, name);
							return 0;
						}
						*numEnd = ']';
						index = indexData->uivalue;
					}
					else
					{
						index = (luint)atoll(num);
						*numEnd = ']';
					}

					array_t *arr = (array_t *)(fieldVal->ovalue);
					if (!arr)
					{
						env_raise_exception(env, exception_null_dereference, name);
						return 0;
					}

					if (index >= arr->length)
					{
						env_raise_exception(env, exception_bad_array_index, "%s at %u", name, index);
						return 0;
					}

					byte_t elemType = value_typeof((value_t *)arr) - lb_object + lb_char - 1;
					valsize = sizeof_type(elemType);
					*flags = 0;
					value_set_type((value_t *)flags, elemType);
					*data = (data_t *)((byte_t *)&arr->data + (valsize * index));
					return 1;
				}
				else
				{

					fieldVal = class_get_static_field(clazz, beg);
					if (!fieldVal)
					{
						env_raise_exception(env, exception_bad_variable_name, name);
						return 0;
					}
					*data = (data_t *)&fieldVal->ovalue;
					*flags = fieldVal->flags;
					return 1;
				}
			}
		}	
	}

	// If we have a '[', we have an array we need to access
	indBeg = strchr(name, '[');
	if (indBeg)
	{
		*indBeg = 0;
		mapNode = map_find((map_t *)env->variables->data, name);
		*indBeg = '[';
		if (!mapNode)
		{
			
			env_raise_exception(env, exception_bad_variable_name, name);
			return 0;
		}

		char *num = indBeg + 1;
		char *numEnd = strchr(num, ']');
		luint index;
		if (!numEnd)
		{
			env_raise_exception(env, exception_bad_variable_name, name);
			return 0;
		}

		*numEnd = 0;
		if (!is_numeric(num))
		{
			data_t *indexData;
			flags_t indexFlags;
			if (!env_resolve_variable(env, num, &indexData, &indexFlags))
			{
				*numEnd = ']';
				env_raise_exception(env, exception_bad_variable_name, name);
				return 0;
			}
			*numEnd = ']';
			index = indexData->uivalue;
		}
		else
		{
			index = (luint)atoll(num);
			*numEnd = ']';
		}

		array_t *arr = (array_t *)(((value_t *)mapNode->value)->ovalue);
		if (!arr)
		{
			env_raise_exception(env, exception_null_dereference, name);
			return 0;
		}


		if (index >= arr->length)
		{
			env_raise_exception(env, exception_bad_array_index, "%s at %u", name, index);
			return 0;
		}

		byte_t elemType = value_typeof((value_t *)arr) - lb_object + lb_char - 1;
		valsize = sizeof_type(elemType);
		*flags = 0;
		value_set_type((value_t *)flags, elemType);
		*data = (data_t *)((byte_t *)&arr->data + (valsize * index));
		return 1;
	}

	// If all the other checks didn't pass, this variable is just normal
	mapNode = map_find((map_t *)env->variables->data, name);
	if (!mapNode)
	{
		env_raise_exception(env, exception_bad_variable_name, name);
		return 0;
	}

	value_t *val = (value_t *)mapNode->value;
	*flags = val->flags;
	*data = (data_t *)&val->ovalue;

	return 1;
}

int env_resolve_object_field(env_t *env, object_t *object, char *name, data_t **data, flags_t *flags)
{
	if (!object)
	{
		env_raise_exception(env, exception_null_dereference, NULL);
		return 0;
	}

	field_t *fieldData;
	char *beg = strchr(name, '.');
	char *bracBeg = strchr(name, '[');
	char *bracEnd = NULL;

	if (bracBeg && (bracBeg < beg || (bracBeg && !beg)))
	{
		bracEnd = strchr(name, ']');
		if (!bracEnd)
		{
			env_raise_exception(env, exception_bad_variable_name, name);
			return 0;
		}
		char *indBegin = bracBeg + 1;

		*bracEnd = 0;
		luint index = (luint)atoll(indBegin);
		

		char buf[100];
		sprintf_s(buf, sizeof(buf), "%u", index);

		if (strcmp(buf, indBegin))
		{
			// Index is possibly a variable

			data_t *indexData;
			flags_t indexFlags;
			if (!env_resolve_variable(env, indBegin, &indexData, &indexFlags))
			{
				*bracEnd = ']';
				env_raise_exception(env, exception_bad_variable_name, name);
				return 0;
			}

			index = indexData->uivalue;
		}

		*bracBeg = 0;
		fieldData = object_get_field_data(object, name);
		if (!fieldData)
		{
			env_raise_exception(env, exception_bad_variable_name, "field %s", name);
			return 0;
		}
		*bracBeg = '[';

		*data = (data_t *)((byte_t *)&object->data + (size_t)fieldData->offset);
		*flags = 0;
		array_t *arr = (array_t *)(*data)->ovalue;
		if (!arr)
		{
			env_raise_exception(env, exception_null_dereference, name);
			return 0;
		}
		
		if (!ARRAY_INDEX_INBOUNDS(arr, index))
		{
			env_raise_exception(env, exception_bad_array_index, "%s at %u", name, index);
			return 0;
		}

		*bracEnd = ']';

		byte_t elemType = value_typeof((value_t *)arr) - lb_object + lb_char - 1;
		SET_TYPE(*flags, elemType);
		size_t elemSize = sizeof_type(elemType);
		*data = array_get_data(arr, index, elemSize);
		return 1;
	}

	// We might need to go into another object's fields if there is a '.'
	if (beg)
	{
		size_t off;
		void *objectData;

		*beg = 0;
		fieldData = object_get_field_data(object, name);
		*beg = '.';

		if (!fieldData)
		{
			env_raise_exception(env, exception_bad_variable_name, name);
			return 0;
		}

		off = (size_t)fieldData->offset;
		objectData = (byte_t *)&object->data + off;

		beg++;
		return env_resolve_object_field(env, *((object_t **)objectData), beg, data, flags);

		
	}
	else
	{
		value_t *val = (value_t *)object;
		byte_t type = value_typeof(val);
		array_t *arr = (array_t *)val;

		switch (type)
		{
		case lb_object:
			fieldData = object_get_field_data(object, name);
			if (!fieldData)
			{
				env_raise_exception(env, exception_bad_variable_name, "field %s.%s", object->clazz->name, name);
				return 0;
			}

			*flags = fieldData->flags;
			*data = (data_t *)((byte_t *)&object->data + (size_t)fieldData->offset);
			return 1;
			break;
		case lb_boolarray:
		case lb_chararray:
		case lb_uchararray:
		case lb_shortarray:
		case lb_ushortarray:
		case lb_intarray:
		case lb_uintarray:
		case lb_longarray:
		case lb_ulongarray:
		case lb_floatarray:
		case lb_doublearray:
		case lb_objectarray:
			if (strcmp(name, "length"))
			{
				env_raise_exception(env, exception_bad_variable_name, "field %s", name);
				return 0;
			}
			*data = (data_t *)&arr->length;
			*flags = 0;
			SET_TYPE(*flags, lb_uint);
			return 1;
			break;
		default:
			env_raise_exception(env, exception_bad_variable_name, name);
			return 0;
			break;
		}
	}
}

int env_resolve_function_name(env_t *env, char *name, function_t **function)
{
	if (!name)
	{
		env_raise_exception(env, exception_function_not_found, name);
		return 0;
	}

	const char *funcname;
	function_t *result;
	
	char *paren = strrchr(name, '(');
	assert(paren);

	*paren = 0;
	char *end = strrchr(name, '.');
	*paren = '(';

	if (end)
	{
		*end = 0;
		data_t *data;
		signed long long flags;
		if (env_resolve_variable(env, name, &data, &flags))
		{
			*end = '.';
			funcname = end + 1;
			byte_t type = TYPEOF(flags);
			if (type != lb_object)
			{
				env_raise_exception(env, exception_function_not_found, name);
				return 0;
			}
			object_t *obj = (object_t *)data->ovalue;
			class_t *parent = obj->clazz;
			result = class_get_function(parent, funcname);
			*function = result;
			return 1;
		}
		else
		{
			class_t *clazz;

			CLEAR_EXCEPTION(env);	

			//*end = '.';
			end = resolve_first_class(env->vm, name, &clazz);

			if (!clazz)
			{
				env_raise_exception(env, exception_class_not_found, name);
				return 0;
			}

			*end = '.';
			funcname = end + 1;

			result = class_get_function(clazz, funcname);
			if (!result)
			{
				env_raise_exception(env, exception_function_not_found, name);
				return 0;
			}
			*function = result;
			return 1;
		}
	}
	else
	{
		class_t *clazz = CURR_FUNC(env)->parentClass;
		result = class_get_function(clazz, name);
		if (!result)
		{
			env_raise_exception(env, exception_function_not_found, name);
			return 0;
		}
		*function = result;
		return 1;
	}

	env_raise_exception(env, exception_function_not_found, name);
	return 0;
}

int env_resolve_dynamic_function_name(env_t *env, char *name, function_t **function, data_t **data, flags_t *flags)
{
	char *funend = strchr(name, '(');
	*funend = 0;
	char *last = strrchr(name, '.');
	*last = 0;
	if (!env_resolve_variable(env, name, data, flags))
	{
		*funend = '(';
		*last = '.';
		return 0;
	}
	*funend = '(';
	*last = '.';

	byte_t type = TYPEOF(*flags);
	if (type != lb_object)
	{
		env_raise_exception(env, exception_function_not_found, name);
		return 0;
	}

	object_t *object = (object_t *)(*data)->ovalue;
	if (!object)
	{
		env_raise_exception(env, exception_null_dereference, name);
		return 0;
	}

	const char *funcName = last + 1;
	*function = class_get_function(object->clazz, funcName);
	if (!(*function))
	{
		env_raise_exception(env, exception_function_not_found, name);
		return 0;
	}

	return 1;
}

int env_run_func_staticv(env_t *env, function_t *function, va_list ls)
{
	int code;
	code = env_handle_static_function_callv(env, function, frame_flag_return_native, ls);
	if (code)
		return code;
	if (!(function->flags & FUNCTION_FLAG_NATIVE))
		code = env_run(env, env->rip);
	return code;
}

int env_run_funcv(env_t *env, function_t *function, object_t *object, va_list ls)
{
	int code;
	code = env_handle_dynamic_function_callv(env, function, frame_flag_return_native,  object, ls);
	if (code)
		return code;
	code = env_run(env, env->rip);
	return code;
}

object_t *env_new_string(env_t *env, const char *cstring)
{
	class_t *stringClass = vm_get_class(env->vm, "lscript.lang.String");
	if (!stringClass)
		return NULL;

	unsigned int len = (unsigned int)strlen(cstring);

	array_t *arr = manager_alloc_array(env->vm->manager, lb_chararray, len);
	if (!arr)
		return NULL;

	memcpy(&arr->data, cstring, len);

	object_t *stringObj = manager_alloc_object(env->vm->manager, stringClass);
	if (!stringObj)
		return NULL;

	function_t *constructor = class_get_function(stringClass, "<init>([C");
	if (!constructor)
		return NULL;

	if (env_run_func(env, constructor, stringObj, arr))
		return NULL;

	return stringObj;
}

array_t *env_new_string_array(env_t *env, unsigned int count, const char *const strings[])
{
	array_t *arr = manager_alloc_array(env->vm->manager, lb_objectarray, count);
	if (!arr)
		return NULL;
	for (unsigned int i = 0; i < count; i++)
		array_set_object(arr, i, env_new_string(env, strings[i]));

	return arr;
}

int env_raise_exceptionv(env_t *env, int exception, const char *format, va_list ls)
{
	CLEAR_EXCEPTION(env);
	env->exception = exception;
	if (format)
		vsprintf_s(env->message, sizeof(env->message), format, ls);
	return exception;
}

int env_get_exception_data(env_t *env, function_t **function, void **location)
{
	if (!env->exception)
		return 0;
	
	*function = CURR_FUNC(env);
	*location = env->cmdStart;

	return env->exception;
}

void env_free(env_t *env)
{
	if (env->vm->flags & vm_flag_verbose)
		printf("Freeing execution environment %p\n", (void *)env);

	FREE(env->stack);
	env->stack = NULL;

	list_iterator_t *lit = list_create_iterator(env->variables);
	lit = list_iterator_next(lit);
	while (lit)
	{
		map_free((map_t *)lit->data, 0);
		lit = list_iterator_next(lit);
	}
	list_iterator_free(lit);
	list_free(env->variables, 0);

	vm_t *vm = env->vm;
	list_t *curr = vm->envs;
	while (curr)
	{
		if (env == curr->data)
		{
			list_t *next = curr->next;
			list_t *prev = curr->prev;
			curr->next = NULL;
			list_free(curr, 0);

			if (next == NULL)
				vm->envsLast = prev;

			if (prev == NULL)
				vm->envs = next;

			break;
		}
		curr = curr->next;
	}

	FREE(env);
}

int class_resolve_filename(vm_t *__restrict vm, const char *__restrict classname, char *filename, size_t filenameLen)
{
	char tempName[MAX_PATH];
	strcpy_s(tempName, sizeof(tempName), classname);

	char *cursor = tempName;
	while (*cursor)
	{
		if (*cursor == '.')
			*cursor = '\\';
		cursor++;
	}

	static const char LB_EXT[] = ".lb";
	memcpy(cursor, LB_EXT, sizeof(LB_EXT));

	FILE *dummy;
	DWORD dwAttrib;
	size_t len;
	char *pathCursor;

	list_t *curr = vm->paths;
	while (curr)
	{
		filename[0] = 0;
		char *pathString = (char *)curr->data;

		pathCursor = filename;
		len = strlen(pathString);
		memcpy(pathCursor, pathString, len);
		pathCursor += len;

		len = strlen(tempName) + 1;
		memcpy(pathCursor, tempName, len);

		dwAttrib = GetFileAttributesA(filename);
		if (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
			return 1;

		curr = curr->next;
	}

	return 0;
}

class_t *class_load_to_vm(vm_t *__restrict vm, class_t *__restrict clazz)
{
	assert(clazz);

	// Create the class object
	class_t *classClass = vm_get_class(vm, CLASS_CLASSNAME);
	assert(classClass);

	object_t *classObject = manager_alloc_object(vm->manager, classClass);
	object_set_ulong(classObject, "handle", (lulong)classClass);

	class_t *classnameClass = vm_get_class(vm, STRING_CLASSNAME);
	assert(classnameClass);

	object_t *classnameObject = manager_alloc_object(vm->manager, classnameClass);

	size_t classnameSize = strlen(clazz->name);
	array_t *classnameCharArray = manager_alloc_array(vm->manager, lb_chararray, classnameSize);
	memcpy(&classnameCharArray->data, clazz->name, classnameSize); // normal memcpy - arrays on manager heap
	object_set_object(classnameObject, "chars", classnameCharArray);

	object_set_object(classObject, "name", classnameObject);

	reference_t *strongClassRef = manager_create_strong_object_reference(vm->manager, classObject);
	map_insert(vm->loadedClassObjects, clazz->name, strongClassRef);

	function_t *staticinit;
	if (staticinit = class_get_function(clazz, "<staticinit>("))
	{
		env_t *env = env_create(vm);
		env_run_func_static(env, staticinit);
		env_free(env);
	}

	return clazz;
}

class_t *class_load_filename_override(vm_t *__restrict vm, const char *__restrict classname,
	const char *__restrict filename, int loadSuperClasses, int loadToVM, int checkExists)
{
	if (checkExists)
	{
		class_t *result = (class_t *)map_at(vm->classes, classname);
		if (result) return result;
	}

	if (vm->flags & vm_flag_verbose)
		printf("Loading class \"%s\".\n", classname);

	class_t *result = vm_load_class_file(vm, filename, loadSuperClasses);
	if (!result)
	{
		if ((vm->flags & vm_flag_verbose) || (vm->flags & vm_flag_verbose_errors))
			printf("Class load error for class \"%s\": Failed to parse binary (resolved to file \"%s\").\n", classname, filename);
		return NULL;
	}

	return loadToVM ? class_load_to_vm(vm, result) : result;
}

static char *resolve_first_class(vm_t *vm, char *name, class_t **result)
{
	char filename[MAX_PATH];
	char classBuild[MAX_PATH];
	size_t strsize;
	const char *start = name;
	char *buildCursor = classBuild;
	char *beg = strchr(name, '.');

	do
	{
		*beg = 0;

		strsize = strlen(start);
		strcpy_s(buildCursor, sizeof(classBuild) - 1 - (buildCursor - classBuild), start);
		buildCursor += strsize;
		*beg = '.';

		if (class_resolve_filename(vm, classBuild, filename, sizeof(filename)))
		{
			*result = class_load_filename_override(vm, classBuild, filename, 1, 1, 1);
			return beg;
		}

		*buildCursor = '.';
		buildCursor++;

		beg++;

		start = beg;
		beg = strchr(beg, '.');
	} while (beg);
	start += strlen(start);

	*result = NULL;
	if (class_resolve_filename(vm, name, filename, sizeof(filename)))
		*result = class_load_filename_override(vm, name, filename, 1, 1, 1);
	return start;
}

class_t *class_load_ext(const char *classname, vm_t *vm)
{
	return vm_load_class(vm, classname);
}

int env_run(env_t *__restrict env, void *__restrict location)
{
	int __retVal;
	env->rip = (byte_t *)location;

	const char *name;			// An arbitrary string to store a name
	data_t *data;				// An arbitrary data_t pointer
	flags_t flags;				// An arbitrary flags_t
	const char *name2;			// An arbitrary string to store a name
	const char *name3;			// An arbitrary string to store a name
	data_t *data2;				// An arbitrary data_t pointer
	flags_t flags2;				// An arbitrary flags_t
	value_t val;				// An arbitrary value
	void *stackAllocLoc;		// A pointer to where a value on the stack was allocated
	function_t *callFunc;		// A pointer to a function which will be called
	byte_t *callFuncArgs;		// A pointer to some bytes which will be the function arguments
	object_t *object;			// A pointer to an object_t used for holding some object
	class_t *clazz;				// A pointer to a class_t used for holding some class
	size_t off;					// An arbitrary value for storing an offset
	byte_t type;				// An arbitrary value for storing a type

	__retVal = exception_none;

	while (env->rip)
	{
		env->cmdStart = env->rip; // Save the start of the current command

		// Switch on the command
		switch (*env->rip)
		{
		case lb_noop:
			// noop command - just move forward 1 byte
			env->rip++;
			break;

		case lb_char:
		case lb_uchar:
		case lb_short:
		case lb_ushort:
		case lb_int:
		case lb_uint:
		case lb_long:
		case lb_ulong:
		case lb_bool:
		case lb_float:
		case lb_object:
		case lb_chararray:
		case lb_uchararray:
		case lb_shortarray:
		case lb_ushortarray:
		case lb_intarray:
		case lb_uintarray:
		case lb_longarray:
		case lb_ulongarray:
		case lb_boolarray:
		case lb_floatarray:
		case lb_doublearray:
		case lb_objectarray:
			// Handle variable declaration

			type = *env->rip;
			env->rip++;
			name = env->rip;
			if (!is_varname_avaliable(env, name))
				EXIT_RUN(env_raise_exception(env, exception_bad_variable_name, name));
			env->rip += strlen(name) + 1;
			val.flags = 0;
			val.lvalue = 0;
			value_set_type(&val, type);
			stackAllocLoc = stack_push(env, &val);
			if (!stackAllocLoc)
				EXIT_RUN(env->exception);
			map_insert((map_t *)env->variables->data, name, stackAllocLoc);
			break;

		case lb_setb:
			// Set variable to literal byte value

			env->rip++;
			name = env->rip;
			if (!env_resolve_variable(env, name, &data, &flags))
				EXIT_RUN(env->exception);
			env->rip += strlen(name) + 1;
			memcpy(data, env->rip, sizeof(byte_t));
			env->rip += sizeof(byte_t);
			break;
		case lb_setw:
			// Set variable to literal word value

			env->rip++;
			name = env->rip;
			if (!env_resolve_variable(env, name, &data, &flags))
				EXIT_RUN(env->exception);
			env->rip += strlen(name) + 1;
			memcpy(data, env->rip, sizeof(word_t));
			env->rip += sizeof(word_t);
			break;
		case lb_setd:
			// Set variable to literal dword value

			env->rip++;
			name = env->rip;
			if (!env_resolve_variable(env, name, &data, &flags))
				EXIT_RUN(env->exception);
			env->rip += strlen(name) + 1;
			memcpy(data, env->rip, sizeof(dword_t));
			env->rip += sizeof(dword_t);
			break;
		case lb_setq:
			// Set variable to literal qword value

			env->rip++;
			name = env->rip;
			if (!env_resolve_variable(env, name, &data, &flags))
				EXIT_RUN(env->exception);
			env->rip += strlen(name) + 1;
			memcpy(data, env->rip, sizeof(qword_t));
			env->rip += sizeof(qword_t);
			break;
		case lb_setr4:
			// Set variable to literal real4 value

			env->rip++;
			name = env->rip;
			if (!env_resolve_variable(env, name, &data, &flags))
				EXIT_RUN(env->exception);
			env->rip += strlen(name) + 1;
			memcpy(data, env->rip, sizeof(real4_t));
			env->rip += sizeof(real4_t);
			break;
		case lb_setr8:
			// Set variable to literal real8 value

			env->rip++;
			name = env->rip;
			if (!env_resolve_variable(env, name, &data, &flags))
				EXIT_RUN(env->exception);
			env->rip += strlen(name) + 1;
			memcpy(data, env->rip, sizeof(real8_t));
			env->rip += sizeof(real8_t);
			break;
		case lb_seto:
			// Set a variable to an object

			env->rip++;
			name = (const char *)env->rip;
			if (!env_resolve_variable(env, name, &data2, &flags))
				EXIT_RUN(env->exception);
			env->rip += strlen(name) + 1;
			switch (*env->rip)
			{
			case lb_new:
				// Create a new object

				env->rip++;

				// Get the class to instantiate
				name2 = (const char *)env->rip;
				clazz = vm_load_class(env->vm, name2); // This function will only load the class if it is not loaded
				if (!clazz)
					EXIT_RUN(env_raise_exception(env, exception_class_not_found, name2));
				env->rip += strlen(name2) + 1;

				// Get the constructor function
				name3 = (const char *)env->rip;
				callFunc = class_get_function(clazz, name3);
				if (!callFunc)
					EXIT_RUN(env_raise_exception(env, exception_function_not_found, name3));
				env->rip += strlen((const char *)env->rip) + 1;

				// Allocate the new object
				object = manager_alloc_object(env->vm->manager, clazz);
				if (!object)
					EXIT_RUN(env_raise_exception(env, exception_out_of_memory, NULL));
				data2->ovalue = object;

				goto handle_dynamic_call_after_resolve; // Call the constructor
				break;
			case lb_char:
			case lb_uchar:
			case lb_short:
			case lb_ushort:
			case lb_int:
			case lb_uint:
			case lb_long:
			case lb_ulong:
			case lb_bool:
			case lb_float:
			case lb_double:
			case lb_object:
				// Create new array

				type = (*env->rip) + 0x0c; // The type to store
				env->rip++;
				switch (*env->rip)
				{
				case lb_value:
					// The size we want to allocate is stored in a variable

					env->rip++;
					if (!env_resolve_variable(env, env->rip, &data, &flags))
						EXIT_RUN(env->exception);
					env->rip += strlen(env->rip) + 1;
					switch (value_typeof((value_t *)&flags))
					{
					case lb_uint:
						data2->ovalue = manager_alloc_array(env->vm->manager, type, data->uivalue);
						break;
					case lb_bool:
					case lb_char:
					case lb_uchar:
					case lb_short:
					case lb_ushort:
					case lb_int:
					case lb_long:
					case lb_ulong:
					case lb_float:
					case lb_double:
					case lb_object:
					case lb_chararray:
					case lb_uchararray:
					case lb_shortarray:
					case lb_ushortarray:
					case lb_intarray:
					case lb_uintarray:
					case lb_longarray:
					case lb_ulongarray:
					case lb_boolarray:
					case lb_floatarray:
					case lb_doublearray:
					case lb_objectarray:
						EXIT_RUN(env_raise_exception(env, exception_illegal_state, "init array requires 32-bit unsigned integral size"));
						break;
					}

					if (!data2->ovalue)
						EXIT_RUN(env_raise_exception(env, exception_out_of_memory, NULL));

					break;
				case lb_dword:
					// The size we want to allocate is in a dword

					data2->ovalue = manager_alloc_array(env->vm->manager, type, *((unsigned int *)(++(env->rip))));
					if (!data2->ovalue)
						EXIT_RUN(env_raise_exception(env, exception_out_of_memory, NULL));
					env->rip += sizeof(unsigned int);
					break;
				default:
					EXIT_RUN(env_raise_exception(env, exception_bad_command, "seto expected value or dword"));
					break;
				}
				break;
			case lb_string:
				// Create new string

				env->rip++;
				data2->ovalue = env_new_string(env, env->rip);
				if (env->exception)
					EXIT_RUN(env->exception);
				env->rip += strlen(env->rip) + 1;
				break;
			case lb_null:
				// Set to null

				data2->ovalue = NULL;
				break;
			default:
				EXIT_RUN(env_raise_exception(env, exception_bad_command, "seto expected type"));
				break;
			}
			break;
		case lb_setv:
			// Set variable to other variable

			env->rip++;

			// Destination variable
			name = env->rip;
			if (!env_resolve_variable(env, name, &data, &flags))
				EXIT_RUN(env->exception);
			env->rip += strlen(name) + 1;

			// Source variable
			name2 = env->rip;
			if (!env_resolve_variable(env, name2, &data2, &flags2))
				EXIT_RUN(env->exception);
			env->rip += strlen(name2) + 1;

			// Set
			if (!static_set(data, flags, data2, flags2))
				EXIT_RUN(env_raise_exception(env, exception_bad_command, "On static set during setv"));
			break;
		case lb_setr:
			// Set variable to the return value of the last function

			env->rip++;

			// Destination variable
			name = env->rip;
			if (!env_resolve_variable(env, name, &data, &flags))
				EXIT_RUN(env->exception);
			env->rip += strlen(name) + 1;

			// Set
			store_return(env, data, flags);
			break;

		case lb_retb:
			// Return literal byte value
			
			env->rip++;
			env->bret = *(byte_t *)env->rip;
			goto general_ret_command_handle;
			break;
		case lb_retw:
			// Return literal word value

			env->rip++;
			env->wret = *(word_t *)env->rip;
			goto general_ret_command_handle;
			break;
		case lb_retd:
			// Return literal dword value

			env->rip++;
			env->dret = *(dword_t *)env->rip;
			goto general_ret_command_handle;
			break;
		case lb_retq:
			// Return literal qword value
			
			env->rip++;
			env->qret = *(qword_t *)env->rip;
			goto general_ret_command_handle;
			break;
		case lb_retr4:
			// Return literal real4 value

			env->rip++;
			env->r4ret = *(real4_t *)env->rip;
			goto general_ret_command_handle;
			break;
		case lb_retr8:
			// Return literal real8 value

			env->rip++;
			env->r8ret = *(real8_t *)env->rip;
			goto general_ret_command_handle;
			break;
		case lb_retv:
			// Return variable value

			env->rip++;
			name = env->rip;
			if (!env_resolve_variable(env, name, &data, &flags))
				EXIT_RUN(env->exception);
			memcpy(&env->vret, data, value_sizeof((value_t *)&flags));
			goto general_ret_command_handle;
			break;
		case lb_ret:
		case lb_retr:
		general_ret_command_handle:
			// Pop stack frame
			
			// Return natively if the reason the function started was due
			// to a native function call
			if (CURR_FLAGS(env) & frame_flag_return_native)
				EXIT_RUN(env_cleanup_call(env, 0));
			if (env_cleanup_call(env, 0))
				EXIT_RUN(env->exception);
			break;

		case lb_static_call:
			// Call a static function

			env->rip++;

			// Find function
			name = env->rip;
			if (!env_resolve_function_name(env, name, &callFunc))
				EXIT_RUN(env->exception);
			env->rip += strlen(name) + 1;
			
			// Get the function arguments as a list
			callFuncArgs = env_gen_call_arg_list(env, callFunc);
			if (!callFuncArgs)
				EXIT_RUN(env->exception);

			// Handle the function call
			if (__retVal = env_handle_static_function_callv(env, callFunc, 0, callFuncArgs))
				EXIT_RUN(__retVal);

			FREE(callFuncArgs);

			break;
		case lb_dynamic_call:
			// Call a dynamic function

			env->rip++;

			// Find funuction
			name = env->rip;
			if (!env_resolve_dynamic_function_name(env, name, &callFunc, &data, &flags))
				EXIT_RUN(env->exception);
			env->rip += strlen(name) + 1;

			object = data->ovalue;

			handle_dynamic_call_after_resolve:

			// Get the function arguments as a list
			callFuncArgs = env_gen_call_arg_list(env, callFunc);
			if (!callFuncArgs)
				EXIT_RUN(env->exception);

			// Handle the function call
			if (__retVal = env_handle_dynamic_function_callv(env, callFunc, 0, object, (va_list)callFuncArgs))
				EXIT_RUN(__retVal);

			FREE(callFuncArgs);
			break;

		case lb_add:
			// add
			
			env->rip++;
			if (!vmm_add(env, &env->rip))
				return env->exception;
			break;
		case lb_sub:
			// subtract

			env->rip++;
			if (!vmm_sub(env, &env->rip))
				return env->exception;
			break;
		case lb_mul:
			// multiply

			env->rip++;
			if (!vmm_mul(env, &env->rip))
				return env->exception;
			break;
		case lb_div:
			// divide

			env->rip++;
			if (!vmm_div(env, &env->rip))
				return env->exception;
			break;
		case lb_mod:
			// modulus

			env->rip++;
			if (!vmm_mod(env, &env->rip))
				return env->exception;
			break;
		case lb_and:
			// bitwise and

			env->rip++;
			if (!vmm_and(env, &env->rip))
				return env->exception;
			break;
		case lb_or:
			// birwise or

			env->rip++;
			if (!vmm_or(env, &env->rip))
				return env->exception;
			break;
		case lb_xor:
			// exclusive or

			env->rip++;
			if (!vmm_xor(env, &env->rip))
				return env->exception;
			break;
		case lb_lsh:
			// left shift

			env->rip++;
			if (!vmm_lsh(env, &env->rip))
				return env->exception;
			break;
		case lb_rsh:
			// right shift

			env->rip++;
			if (!vmm_rsh(env, &env->rip))
				return env->exception;
			break;

		case lb_neg:
			// negate

			env->rip++;
			if (!vmm_neg(env, &env->rip))
				return env->exception;
			break;
		case lb_not:
			// bitwise not

			env->rip++;
			if (!vmm_not(env, &env->rip))
				return env->exception;
			break;

		case lb_while:
			// while loop

			// Perform comparison
			if (!vmc_compare(env, &env->rip))
			{
				if (env->exception)
					EXIT_RUN(env->exception);

				// exit while loop
				env->rip = CURR_FUNC(env)->parentClass->data + *((size_t *)env->rip);
			}
			else
				env->rip += sizeof(size_t); // comparison suceeds, proceed into body
			break;

		case lb_if:
			// if or elif statement

 			if (handle_if(env))
				EXIT_RUN(env->exception);
			break;
		case lb_elif:
		case lb_else:
		case lb_end:
			// end command

			env->rip++;

			off = *((size_t *)env->rip);
			if (off == (size_t)-1)
				env->rip += sizeof(size_t); // -1 indicates to proceed directly forward
			else
				env->rip = CURR_FUNC(env)->parentClass->data + off; // otherwise, an offset from class data
			break;

		case lb_push:
			// Pushes 1 qword onto the stack

			env->rip++;
			switch (*(env->rip))
			{
			case lb_ret:
				env->rip++;
				{
					qword_t *allocated = (qword_t *)stack_alloc(env, 1);
					if (!allocated)
						EXIT_RUN(env->exception);
					*allocated = env->qret;
				}
				break;
			case lb_value:
				env->rip++;
				{
					if (!env_resolve_variable(env, env->rip, &data, &flags))
						EXIT_RUN(env->exception);
					qword_t *allocated = (qword_t *)stack_alloc(env, 1);
					if (!allocated)
						EXIT_RUN(env->exception);
					*allocated = data->ulvalue;
				}
				break;
			default:
				EXIT_RUN(env_raise_exception(env, exception_bad_command, "Invalid push format, must be either ret or value"));
			}

			break;
		case lb_pop:
			// Pops 1 qword off the stack

			env->rip++;
			switch (*(env->rip))
			{
			case lb_null:
				if (!stack_pop(env, 1, NULL))
					EXIT_RUN(env->exception);
				break;
			default:
				EXIT_RUN(env_raise_exception(env, exception_bad_command, "Invalid pop format, must be null"));
			}
			break;

		case lb_castc:
		case lb_castuc:
		case lb_casts:
		case lb_castus:
		case lb_casti:
		case lb_castui:
		case lb_castl:
		case lb_castul:
		case lb_castb:
		case lb_castf:
		case lb_castd:
			// Cast variables

			if (!handle_cast(env, &env->rip))
				EXIT_RUN(env->exception);
			break;

		default:
			EXIT_RUN(env_raise_exception(env, exception_bad_command, "Unknown instruction 0x%02x", (unsigned int)(*env->rip)));
		}
	}

done_call:

	return __retVal;
}

inline int env_create_stack_frame(env_t *__restrict env, function_t *__restrict function, flags_t flags)
{
	size_t *stackframe = stack_alloc(env, 4);
	if (!stackframe)
		return env->exception;

	*(stackframe) = (size_t)env->rbp;
	*(stackframe + 1) = (size_t)env->rip;
	*(stackframe + 2) = (size_t)function;
	*(stackframe + 3) = (size_t)flags;

	env->rbp = (byte_t *)(stackframe + 4);
	return exception_none;
}

inline int env_cleanup_call(env_t *__restrict env, int onlyStackCleanup)
{
	if (!onlyStackCleanup)
	{
		map_t *vars = (map_t *)env->variables->data;
		if (!vars)
			return env_raise_exception(env, exception_illegal_state, NULL);

		map_free(vars, 0);

		// Remove the map from the list
		list_t *prev = env->variables->prev;
		env->variables->prev = NULL;
		list_free(env->variables, 0);
		env->variables = prev;
		env->variables->next = NULL;
	}

	// Restore the fake registers from the previous call
	size_t *stackframe = (size_t *)env->rbp - 4;
	
	env->rip = FRAME_RIP(env->rbp);
	env->rsp = env->rbp;
	env->rbp = (byte_t *)(*stackframe);
		//(byte_t *)(*(stackframe + 1));
	//env->rip = ((byte_t *)env->rbp) + sizeof(void *);

	return exception_none;
}

int env_handle_static_function_callv(env_t *__restrict env, function_t *__restrict function, frame_flags_t flags, va_list ls)
{
	if (env_create_stack_frame(env, function, flags) != exception_none)
		return env->exception;

	if (function->flags & FUNCTION_FLAG_NATIVE)
	{
		if (!function->location)
		{
			if (!try_link_function(env->vm, function))
				return env_raise_exception(env, exception_link_error, function->name);
		}

		void *args = CALLOC(function->numargs + 2, sizeof(qword_t));

		//MALLOC((function->numargs * sizeof(qword_t)) + (2 * sizeof(qword_t)));
		if (!args)
			return env_raise_exception(env, exception_vm_error, "allocate native function arguments");

		void **temp = (void **)args;
		temp[0] = env;
		temp[1] = function->parentClass;

		byte_t *types = NULL;
		
		if (function->numargs > 0)
		{
			types = (byte_t *)MALLOC(sizeof(byte_t) * function->numargs);
			if (!types)
			{
				FREE(args);
				return env_raise_exception(env, exception_vm_error, "allocate native function arguments");
			}

			if (function->numargs > 0)
			{
				size_t i;
				for (i = 0; i < function->numargs; i++)
				{
					switch ((byte_t)map_at(function->argTypes, function->args[i]))
					{
					case lb_char:
					case lb_uchar:
					case lb_bool:
						types[i] = lb_byte;
						break;
					case lb_short:
					case lb_ushort:
						types[i] = lb_word;
						break;
					case lb_int:
					case lb_uint:
						types[i] = lb_dword;
						break;
					case lb_long:
					case lb_ulong:
					case lb_object:
					case lb_boolarray:
					case lb_chararray:
					case lb_uchararray:
					case lb_shortarray:
					case lb_ushortarray:
					case lb_intarray:
					case lb_uintarray:
					case lb_longarray:
					case lb_ulongarray:
					case lb_floatarray:
					case lb_doublearray:
					case lb_objectarray:
						types[i] = lb_qword;
						break;
					case lb_float:
						types[i] = lb_real4;
						break;
					case lb_double:
						types[i] = lb_real8;
						break;
					}
				}
			}

			size_t *outArgsCursor = (size_t *)args;
			char *lsCursor = ls;
			outArgsCursor += 2;
			for (size_t i = 0; i < function->numargs; i++, outArgsCursor++)
			{
				switch (types[i])
				{
				case lb_byte:
					*((byte_t *)outArgsCursor) = *((byte_t *)lsCursor);
					lsCursor += sizeof(byte_t);
					break;
				case lb_word:
					*((word_t *)outArgsCursor) = *((word_t *)*lsCursor);
					lsCursor += sizeof(word_t);
					break;
				case lb_dword:
					*((dword_t *)outArgsCursor) = *((dword_t *)lsCursor);
					lsCursor += sizeof(dword_t);
					break;
				case lb_qword:
					*((qword_t *)outArgsCursor) = *((qword_t *)lsCursor);
					lsCursor += sizeof(qword_t);
					break;
				case lb_real4:
					*((real4_t *)outArgsCursor) = *((real4_t *)lsCursor);
					lsCursor += sizeof(real4_t);
					break;
				case lb_real8:
					*((real8_t *)outArgsCursor) = *((real8_t *)lsCursor);
					lsCursor += sizeof(real8_t);
					break;
				}
			}
		}

		env->qret = vm_call_extern_asm(function->numargs + 2, NULL, args, function->location);

		FREE(types);
		FREE(args);

		env_cleanup_call(env, 1);

		return env->exception;
	}
	else
	{
		if (function->flags & FUNCTION_FLAG_ABSTRACT)
		{
			assert(!function->location);
			env_raise_exception(env, exception_illegal_state, "attempting to call abstract virtual function %s.%s", function->parentClass->name, function->qualifiedName);
			return env->exception;
		}
		assert(function->location);

		env->variables->next = list_create();
		env->variables->next->prev = env->variables;
		env->variables = env->variables->next;
		env->variables->data = map_create(16, string_hash_func, string_compare_func, NULL, NULL, NULL);

		const char *argname;
		byte_t type;
		size_t size;
		value_t val;
		void *loc;
		for (size_t i = 0; i < function->numargs; i++)
		{
			argname = function->args[i];
			type = (byte_t)map_at(function->argTypes, argname);
			size = sizeof_type(type);
			val.flags = 0;
			value_set_type(&val, type);
			memcpy(&val.ovalue, ls, size);
			ls += size;

			if (!(loc = stack_push(env, &val)))
				return env->exception;

			map_insert((map_t *)env->variables->data, argname, loc);
		}

		// Register static variables
		map_iterator_t *sfieldIt = map_create_iterator(function->parentClass->staticFields);
		while (sfieldIt->node)
		{
			const char *fieldName = (const char *)sfieldIt->key;
			if (!map_find((map_t *)env->variables->data, fieldName))
				map_insert((map_t *)env->variables->data, fieldName, sfieldIt->value);
			sfieldIt = map_iterator_next(sfieldIt);
		}
		map_iterator_free(sfieldIt);

		env->rip = function->location;
		return exception_none;
	}
}

int env_handle_dynamic_function_callv(env_t *__restrict env, function_t *__restrict function, frame_flags_t flags, object_t *object, va_list ls)
{
	// push the arg list to the stack

	if (env_create_stack_frame(env, function, flags) != exception_none)
		return env->exception;

	env->variables->next = list_create();
	env->variables->next->prev = env->variables;
	env->variables = env->variables->next;
	env->variables->data = map_create(16, string_hash_func, string_compare_func, NULL, NULL, NULL);

	for (size_t i = 0; i < function->numargs; i++)
	{
		const char *argname = function->args[i];
		byte_t type = (byte_t)map_at(function->argTypes, argname);
		size_t size = sizeof_type(type);
		value_t val;
		val.flags = 0;
		value_set_type(&val, type);
		memcpy(&val.ovalue, ls, size);
		ls += size;

		void *loc;
		if (!(loc = stack_push(env, &val)))
			return env->exception;

		map_insert((map_t *)env->variables->data, argname, loc);
	}

	// Register static variables
	map_iterator_t *sfieldIt = map_create_iterator(function->parentClass->staticFields);
	while (sfieldIt->node)
	{
		const char *fieldName = (const char *)sfieldIt->key;
		if (!map_find((map_t *)env->variables->data, fieldName))
			map_insert((map_t *)env->variables->data, fieldName, sfieldIt->value);
		sfieldIt = map_iterator_next(sfieldIt);
	}
	map_iterator_free(sfieldIt);

	// Register "this" variable
	void *thisLoc;
	value_t thisVal;
	thisVal.flags = 0;
	value_set_type(&thisVal, lb_object);
	thisVal.ovalue = object;
	if (!(thisLoc = stack_push(env, &thisVal)))
		return env->exception;
	map_insert((map_t *)env->variables->data, "this", thisLoc);

	// At some point, add the fields to possible variables to reference, but for now
	// just require this.<field>
	/*map_iterator_t *mit = map_create_iterator(object->clazz->fields);
	while (mit->node)
	{
		const char *fieldName = (const char *)mit->key;
		map_insert((map_t *)env->variables->data, fieldName, mit->value);
	}
	map_iterator_free(mit);*/

	env->rip = (byte_t *)function->location;
	return exception_none;
}

va_list env_gen_call_arg_list(env_t *env, function_t *function)
{
	byte_t *result;

	map_iterator_t *mip;
	byte_t callArgType;
	size_t callArgSize;
	byte_t *cursor;

	data_t *data;
	flags_t flags;
	byte_t valueType;
	size_t valueSize;
	size_t moveSize;

	result = MALLOC(function->argSize);
	if (!result)
	{
		env_raise_exception(env, exception_out_of_memory, "on malloc call arg list");
		return NULL;
	}

	if (function->argSize == 0)
		return (va_list)result;

	cursor = result;
	
	mip = map_create_iterator(function->argTypes);
	while (mip->node)
	{
		callArgType = (byte_t)mip->value;

		switch (*(env->rip))
		{
		case lb_byte:
			env->rip++;
			*cursor = *(env->rip);
			cursor += 1;
			env->rip += 1;
			break;
		case lb_word:
			env->rip++;
			*((word_t *)cursor) = *((word_t *)env->rip);
			cursor += 2;
			env->rip += 2;
			break;
		case lb_dword:
			env->rip++;
			*((dword_t *)cursor) = *((dword_t *)env->rip);
			cursor += 4;
			env->rip += 4;
			break;
		case lb_qword:
			env->rip++;
			*((qword_t *)cursor) = *((qword_t *)env->rip);
			cursor += 8;
			env->rip += 8;
			break;
		case lb_ret:
			env->rip++;
			switch (callArgType)
			{
			case lb_bool:
			case lb_char:
			case lb_uchar:
				*((byte_t *)cursor) = env->bret;
				cursor += 1;
				break;
			case lb_short:
			case lb_ushort:
				*((word_t *)cursor) = env->wret;
				cursor += 2;
				break;
			case lb_int:
			case lb_uint:
			case lb_float:
				*((dword_t *)cursor) = env->dret;
				break;
			case lb_long:
			case lb_ulong:
			case lb_double:
			case lb_object:
			case lb_boolarray:
			case lb_chararray:
			case lb_uchararray:
			case lb_shortarray:
			case lb_ushortarray:
			case lb_intarray:
			case lb_uintarray:
			case lb_longarray:
			case lb_ulongarray:
			case lb_floatarray:
			case lb_doublearray:
			case lb_objectarray:
				*((qword_t *)cursor) = env->qret;
				break;
			default:
				FREE(result);
				env_raise_exception(env, exception_bad_command, "dynamic_call");
				return NULL;
				break;
			}
			break;
		case lb_string:
			env->rip++;
			*((qword_t *)cursor) = (qword_t)env_new_string(env, env->rip);
			if (env->exception)
				return NULL;
			env->rip += strlen(env->rip) + 1;
			cursor += 8;
			break;
		case lb_value:
			env->rip++;
			if (!env_resolve_variable(env, env->rip, &data, &flags))
			{
				FREE(result);
				return NULL;
			}
			valueType = TYPEOF(flags);
			valueSize = sizeof_type(valueType);
			switch (valueType)
			{
			case lb_char:
			case lb_uchar:
				//MEMCPY(cursor, &data->cvalue, 1);
				*cursor = data->cvalue;
				cursor += 1;
				//counter += 1;
				break;
			case lb_short:
			case lb_ushort:
				//MEMCPY(cursor, &data->svalue, min(valueSize, 2));
				*((lshort *)cursor) = data->svalue;
				cursor += 2;
				//counter += 2;
				break;
			case lb_int:
			case lb_uint:
				//MEMCPY(cursor, &data->ivalue, min(valueSize, 4));
				*((lint *)cursor) = data->ivalue;
				cursor += 4;
				//counter += 4;
				break;
			case lb_long:
			case lb_ulong:
				//MEMCPY(cursor, &data->lvalue, min(valueSize, 8));
				*((llong *)cursor) = data->lvalue;
				cursor += 8;
				//counter += 8;
				break;
			case lb_bool:
				//MEMCPY(cursor, &data->bvalue, min(valueSize, 1));
				*((lbool *)cursor) = data->bvalue;
				cursor += 1;
				//counter += 1;
				break;
			case lb_float:
				//MEMCPY(cursor, &data->fvalue, min(valueSize, 4));
				*((lfloat *)cursor) = data->fvalue;
				cursor += 4;
				//counter += 4;
				break;
			case lb_double:
				//MEMCPY(cursor, &data->dvalue, min(valueSize, 8));
				*((ldouble *)cursor) = data->dvalue;
				cursor += 8;
				//counter += 8;
				break;
			case lb_object:
			case lb_chararray:
			case lb_uchararray:
			case lb_shortarray:
			case lb_ushortarray:
			case lb_intarray:
			case lb_uintarray:
			case lb_longarray:
			case lb_ulongarray:
			case lb_boolarray:
			case lb_floatarray:
			case lb_doublearray:
			case lb_objectarray:
				//MEMCPY(cursor, &data->ovalue, min(valueSize, 8));
				*((lobject *)cursor) = data->ovalue;
				cursor += 8;
				//counter += 8;
				break;
			}
			env->rip += strlen(env->rip) + 1;
			break;
		default:
			FREE(result);
			env_raise_exception(env, exception_bad_command, "dynamic_call");
			return NULL;
			break;
		}

		mip = map_iterator_next(mip);
	}
	map_iterator_free(mip);

	return (va_list)result;
}

inline void env_free_call_arg_list(env_t *env, va_list callArgs)
{
	FREE(callArgs);
}

void *stack_push(env_t *env, value_t *value)
{
	size_t *mem = (size_t *)stack_alloc(env, 2);
	if (!mem)
		return NULL;
	*mem = value->flags;
	memcpy(mem + 1, &value->ovalue, value_sizeof(value));
	return mem;
}

void *stack_alloc(env_t *env, size_t words)
{
	env->rsp -= words * sizeof(size_t);
	if (env->rsp <= env->stack)
	{
		env->rsp += words * sizeof(size_t);
		env_raise_exception(env, exception_stack_overflow, NULL);
		return NULL;
	}
	return env->rsp;
}

static int stack_pop(env_t *env, size_t words, qword_t *dstWords)
{
	if (dstWords)
		memcpy(dstWords, env->rsp, words * sizeof(size_t));
	env->rsp -= words * sizeof(size_t);
	return 1;
}

int is_varname_avaliable(env_t *env, const char *name)
{
	map_node_t *mapNode = map_find((map_t *)env->variables->data, name);
	if (mapNode)
		return 0;
	return 1;
}

int static_set(data_t *dst, flags_t dstFlags, data_t *src, flags_t srcFlags)
{
	byte_t dstType, srcType;
	dstType = value_typeof((value_t *)&dstFlags);
	srcType = value_typeof((value_t *)&srcFlags);

	switch (srcType)
	{
	case lb_char:
		return cast_char(&src->cvalue, dstType, dst);
		break;
	case lb_uchar:
		return cast_uchar(&src->ucvalue, dstType, dst);
		break;
	case lb_short:
		return cast_short(&src->svalue, dstType, dst);
		break;
	case lb_ushort:
		return cast_ushort(&src->usvalue, dstType, dst);
		break;
	case lb_int:
		return cast_int(&src->ivalue, dstType, dst);
		break;
	case lb_uint:
		return cast_uint(&src->uivalue, dstType, dst);
		break;
	case lb_long:
		return cast_long(&src->lvalue, dstType, dst);
		break;
	case lb_ulong:
		return cast_ulong(&src->ulvalue, dstType, dst);
		break;
	case lb_bool:
		return cast_bool(&src->bvalue, dstType, dst);
		break;
	case lb_float:
		return cast_float(&src->fvalue, dstType, dst);
		break;
	case lb_double:
		return cast_double(&src->dvalue, dstType, dst);
		break;
	case lb_object:
	case lb_boolarray:
	case lb_chararray:
	case lb_uchararray:
	case lb_shortarray:
	case lb_ushortarray:
	case lb_intarray:
	case lb_uintarray:
	case lb_longarray:
	case lb_ulongarray:
	case lb_floatarray:
	case lb_doublearray:
	case lb_objectarray:
		if (dstType != srcType)
			return 0;
		dst->ovalue = src->ovalue;
		return 1;
		break;
	default:
		return 0;
	}
}

int try_link_function(vm_t *__restrict vm, function_t *__restrict func)
{
#if defined(_WIN32)
	char decName[512];
	sprintf_s(decName, sizeof(decName), "%s_%s", func->parentClass->safeName, func->name);
	for (size_t i = 0; i < vm->libraryCount; i++)
	{
		HMODULE hModule = vm->hLibraries[i];
		if (hModule)
		{
			func->location = GetProcAddress(hModule, decName);
			if (func->location)
				return 1;
		}
	}
#else
#endif
	return 0;
}

void vm_start_routine(start_args_t *args)
{
	class_t *clazz = NULL;
	function_t *func = NULL;
	map_iterator_t *mit;
	vm_t *vm = args->vm;

	mit = map_create_iterator(vm->classes);
	while (mit->node)
	{
		clazz = (class_t *)mit->value;
		func = class_get_function(clazz, "main([Llscript.lang.String;");
		if (func)
			break;
		mit = map_iterator_next(mit);
	}
	map_iterator_free(mit);

	if (func)
	{
		env_t *env = env_create(vm);
		if (env)
		{
			int exception = env_run_func_static(env, func, args->args);
			if (exception)
			{
				env_snapshot_t *ss = env_take_snapshot(env);

				putc('\n', stdout);
				if (env->message[0])
					printf("Internal exception %s raised with message \"%s\"\n", g_exceptionStrings[exception], env->message);
				else
					printf("Internal exception %s raised\n", g_exceptionStrings[exception]);
				 
				printf("Outputting known information up to exception:\n");
				printf("Exception occurred during execution in environment %p\n", env);
				
				function_t *exceptionFunc = ss->function.handle;
				/*printf("Top stack frame parameters of suspect environment:\n");
				printf("(rbp + 0) (last stack frame rbp) = %p\n", *((size_t **)env->rbp));
				printf("(rbp + 1) (last stack frame rip) = %p\n", *((size_t **)env->rbp + 1));
				printf("(rbp + 2) (current function ptr) = %p\n\n", *((size_t **)env->rbp + 2));*/

				printf("Top stack frame points to function %p -> {\n", exceptionFunc);
				printf("\tname = \"%s\"\n", exceptionFunc->name);
				printf("\tqualifiedName = \"%s\"\n", exceptionFunc->qualifiedName);
				printf("\tparentClass = \"%s\"\n", exceptionFunc->parentClass->name);
				printf("\trelativeLocation = %p\n", (void *)ss->function.relativeLocation);
				printf("}\n");

				printf("Exception location relative to function: %p\n", (void *)ss->exec.execFuncOffset);

				print_stack_trace(stdout, env, (vm->flags & vm_flag_verbose) || (vm->flags & vm_flag_verbose_errors));

				if ((vm->flags & vm_flag_verbose) || (vm->flags & vm_flag_verbose_errors))
				{
					printf("\nVariables of suspect execution environment:\n");
					printf("rip = %p, cmdStart = %p, vm = %p\n", env->rip, env->cmdStart, env->vm);
					printf("stack = %p, rsp = %p, rbp = %p\n", env->stack, env->rsp, env->rbp);
					printf("variables = %p, exception = %d, exceptionMessage = %p\n", env->variables, env->exception, env->message);
					printf("bret = %u, wret = %u, dret = %u\n", env->bret, env->wret, env->dret);
					printf("qret = %llu, r4ret = %f, r8ret = %f\n", env->qret, (real8_t)env->r4ret, env->r8ret);
					printf("vret = %p\n\n", env->vret);

					printf("Variables of virtual machine:\n");
					printf("VM address: %p\n", vm);
					printf("envs = %p, envsLast = %p, classes = %p\n", vm->envs, vm->envsLast, vm->classes);
					printf("manager = %p, stackSize = %llu, properties = %p\n", vm->manager, vm->stackSize, vm->properties);
					printf("paths = %p, hLibraries = %p, hVMThread = %p\n", vm->paths, vm->hLibraries, vm->hVMThread);
					printf("dwPadding = %u, libraryCount = %llu, flags = %llu\n\n", vm->dwPadding, vm->libraryCount, vm->flags);
				}

				/*printf("Searching for debug information on classpath...\n");
				char targetName[MAX_PATH];
				char fullPath[MAX_PATH];

				sprintf_s(targetName, MAX_PATH, "%s.lds", exceptionFunc->parentClass->name);

				debug_t *debug = NULL;
				list_t *curr = vm->paths;
				while (curr)
				{
					sprintf_s(fullPath, MAX_PATH, "%s%s", (char *)curr->data, targetName);
					FILE *dummy;
					fopen_s(&dummy, fullPath, "rb");
					if (dummy)
					{
						fclose(dummy);
						debug = load_debug(fullPath);
						break;
					}
					curr = curr->next;
				}

				if (debug)
				{
					printf("Found debug information for class \"%s\"\n", exceptionFunc->parentClass->name);
					printf("Exception occurred in source file %s\n", debug->srcFile);
					unsigned int binOffInt = (unsigned int)(env->cmdStart - exceptionFunc->parentClass->data);
					debug_elem_t *elem = find_debug_elem(debug, binOffInt);
					if (elem)
						printf("Exception occured around: %s.%d\n", debug->srcFile, elem->srcLine);
					else
						printf("Could not find mapping from exception location to source location.\n");
					/*debug_elem_t *start = debug->first;
					while (start < debug->last)
					{
						if (start->binOff >= binOffInt)
						{
							printf("Exception occured around: %s.%d\n", debug->srcFile, start->srcLine);
							break;
						}
						start++;
					}
					if (start >= debug->last)
						printf("Could not find mapping from exception location to source location.\n");
				}
				else
				{
					printf("Could not find debug information for class \"%s\"\n", exceptionFunc->parentClass->name);
				}*/
			}
			env_free(env);
		}
	}
}

void print_stack_trace(FILE *file, env_t *env, int printVars)
{
	byte_t *bottomStack = env->stack + env->vm->stackSize;
	byte_t *rbp = env->rbp;
	function_t *func;
	class_t *clazz;
	unsigned int intOff;
	debug_elem_t *elem;

	while (rbp < bottomStack)
	{
		func = FRAME_FUNC(rbp);
		clazz = func->parentClass;

		if (clazz->debug)
		{
			intOff = (unsigned int)(FRAME_RIP(rbp) - clazz->data);
			elem = find_debug_elem(clazz->debug, intOff);
			if (elem)
			{
				fprintf(file, "%s.%d\n", clazz->debug->srcFile, elem->srcLine);
			}
			else
			{
				fprintf(file, "<class %s>.<function %s>\n", clazz->name, func->qualifiedName);
			}
		}
		else
		{
			fprintf(file, "<class %s>.<function %s>\n", clazz->name, func->qualifiedName);
		}

		rbp = PREV_FRAME(rbp);
	}
}

size_t default_write_stdout(const char *buf, size_t count)
{
	return fwrite(buf, sizeof(char), count, stdout);
}

size_t default_write_stderr(const char *buf, size_t count)
{
	return fwrite(buf, sizeof(char), count, stderr);
}

size_t default_read_stdin(char *buf, size_t size, size_t count)
{
	return fread_s(buf, size, sizeof(char), count, stdin);
}

static char default_read_char_stdin()
{
	return (char)fgetc(stdin);
}
