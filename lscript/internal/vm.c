#include "vm.h"

#include <stdio.h>
#include <string.h>

#include "mem_debug.h"
#include "cast.h"
#include "value.h"
#include "vm_math.h"
#include "vm_compare.h"

#define CURR_CLASS(env) (*(((class_t**)(env)->rbp)+2))

typedef struct control_s control_t;
struct control_s
{
	byte_t *loc;
	byte_t type;
	byte_t padding[7];
};

static class_t *class_load_ext(const char *classname, vm_t *vm);

static int env_run(env_t *env, void *location);
static int env_cleanup_call(env_t *env);

static void *stack_push(env_t *env, value_t *value);
static int stack_pop(env_t *env, flags_t type);

static int is_varname_avaliable(env_t *env, const char *name);

static int static_set(data_t *dst, flags_t dstFlags, data_t *src, flags_t srcFlags);

static int try_link_function(vm_t *vm, function_t *func);

static int equals_ignore_case(const char *s1, const char *s2);

/*
Implemented in hooks.asm
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

static inline int handle_if(env_t *env, byte_t **counterPtr)
{
	if (!vmc_compare(env, counterPtr))
	{
		if (env->exception)
			return env->exception;
		class_t *c = CURR_CLASS(env);
		*counterPtr = c->data + *((size_t *)(*counterPtr));
	}
	else
		*counterPtr += sizeof(size_t);
	return 0;
}

vm_t *vm_create(size_t heapSize, size_t stackSize, int argc, const char *const argv[])
{
	vm_t *vm = (vm_t *)MALLOC(sizeof(vm_t));
	if (!vm)
		return NULL;

	vm->classes = map_create(16, string_hash_func, string_compare_func, string_copy_func, NULL, (free_func_t)free);
	if (!vm->classes)
	{
		FREE(vm);
		return NULL;
	}

	vm->envs = list_create();
	if (!vm->envs)
	{
		map_free(vm->classes, 0);
		FREE(vm);
		return NULL;
	}

	vm->envs->data = NULL;

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
	vm->hLibraries[0] = GetModuleHandleA(NULL);
#else
#endif

	for (int i = 0; i < argc; i++)
	{
		if (equals_ignore_case("-p", argv[i]))
		{
			i++;
			if (i < argc)
			{
				vm_add_path(vm, argv[i]);
			}
		}
	}

	class_t *stringClass = vm_load_class(vm, "String");
	if (!stringClass)
	{
		vm_free(vm);
		return NULL;
	}

	class_t *systemClass = vm_load_class(vm, "System");
	if (!systemClass)
	{
		vm_free(vm);
		return NULL;
	}

	class_t *fileoutputstreamClass = vm_load_class(vm, "FileOutputStream");
	if (!fileoutputstreamClass)
	{
		vm_free(vm);
		return NULL;
	}

	value_t *systemStdout = class_get_static_field(systemClass, "stdout");
	if (!systemStdout)
	{
		vm_free(vm);
		return NULL;
	}

	value_t *systemStderr = class_get_static_field(systemClass, "stderr");
	if (!systemStderr)
	{
		vm_free(vm);
		return NULL;
	}

	value_t *systemStdin = class_get_static_field(systemClass, "stdin");
	if (!systemStdin)
	{
		vm_free(vm);
		return NULL;
	}

	object_t *stdoutVal = manager_alloc_object(vm->manager, fileoutputstreamClass);
	if (!stdoutVal)
	{
		vm_free(vm);
		return NULL;
	}
	systemStdout->ovalue = stdoutVal;

	object_t *stderrVal = manager_alloc_object(vm->manager, fileoutputstreamClass);
	if (!stderrVal)
	{
		vm_free(vm);
		return NULL;
	}
	systemStderr->ovalue = stderrVal;

	object_set_ulong(stdoutVal, "handle", (lulong)stdout);
	object_set_ulong(stderrVal, "handle", (lulong)stderr);

	return vm;
}

class_t *vm_get_class(vm_t *vm, const char *classname)
{
	return (class_t *)map_at(vm->classes, classname);
}

class_t *vm_load_class(vm_t *vm, const char *classname)
{
	class_t *result;
	if (result = vm_get_class(vm, classname))
		return result;

	size_t classnameSize = strlen(classname) + 1;
	char *tempName = (char *)MALLOC(classnameSize);
	if (!tempName)
		return NULL;
	MEMCPY(tempName, classname, classnameSize);

	char *cursor = tempName;
	while (*cursor)
	{
		if (*cursor == '.')
			*cursor = '\\';
		cursor++;
	}

#if defined(_WIN32)
	char fullpath[MAX_PATH];
	FILE *dummy;

	list_t *curr = vm->paths;
	while (curr)
	{
		fullpath[0] = 0;
		char *pathString = (char *)curr->data;
		strcat_s(fullpath, sizeof(fullpath), pathString);
		strcat_s(fullpath, sizeof(fullpath), tempName);
		fopen_s(&dummy, fullpath, "rb");
		if (dummy)
		{
			fclose(dummy);
			result = vm_load_class_file(vm, fullpath);
			break;
		}
		strcat_s(fullpath, sizeof(fullpath), ".lb");
		fopen_s(&dummy, fullpath, "rb");
		if (dummy)
		{
			fclose(dummy);
			result = vm_load_class_file(vm, fullpath);
			break;
		}

		curr = curr->next;
	}
#endif
	FREE(tempName);

	return result;
}

class_t *vm_load_class_file(vm_t *vm, const char *filename)
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

	return vm_load_class_binary(vm, binary, size);
}

class_t *vm_load_class_binary(vm_t *vm, const byte_t *binary, size_t size)
{
	class_t *clazz = class_load(binary, size, (classloadproc_t)class_load_ext, vm);
	if (clazz)
		map_insert(vm->classes, clazz->name, clazz);
	return clazz;
}

void vm_add_path(vm_t *vm, const char *path)
{
	list_t *node = vm->paths;
	while (node->next)
		node = node->next;
	size_t size = strlen(path) + 1;
	char *buf = (char *)MALLOC(size);
	if (buf)
	{
		MEMCPY(buf, path, size);
		list_insert(node, buf);
	}
}

int vm_load_library(vm_t *vm, const char *libpath)
{
#if defined(_WIN32)
	char buf[MAX_PATH];
	sprintf_s(buf, sizeof(buf), "%s.dll", libpath);
	for (size_t i = 0; i < vm->libraryCount; i++)
	{
		if (!vm->hLibraries[i])
		{
			HINSTANCE hinstLib;
			hinstLib = LoadLibraryA(buf);
			if (!hinstLib)
				return 0;
			vm->hLibraries = hinstLib;
			return 1;
		}
	}
#else
#endif
	return 0;
}

void vm_free(vm_t *vm)
{
	list_iterator_t *lit = list_create_iterator(vm->envs);
	while (lit)
	{
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

#if defined(_WIN32)
	for (size_t i = 0; i < vm->libraryCount; i++)
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

env_t *env_create(vm_t *vm)
{
	env_t *env = (env_t *)MALLOC(sizeof(env_t));
	if (!env)
		return NULL;

	env->stack = MALLOC(vm->stackSize);
	if (!env->stack)
	{
		FREE(env);
		return NULL;
	}
	env->rsp = env->stack;
	env->rbp = env->rsp;

	env->variables = list_create();
	env->variables->data = NULL;


	env->variables->data = 0xdeadcafedeadcafe;

	env->rip = NULL;
	env->vm = vm;
	env->exception = exception_none;
	env->exceptionDesc = 0;
	env->qret = 0;

	value_t val;
	val.flags = 0;
	val.ovalue = (lobject)0xdeadbeefdeadbeef;
	value_set_type(&val, lb_object);
	stack_push(env, &val);

	return env;
}

int env_resolve_variable(env_t *env, const char *name, data_t **data, flags_t *flags)
{
	map_node_t *mapNode;
	char *beg = strchr(name, '.');

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
			class_t *clazz = vm_load_class(env->vm, name);
			*beg = '.';
			beg++;
			if (!clazz)
			{
				env->exception = exception_bad_variable_name;
				return 0;
			}
			value_t *fieldVal;

			char *nbeg = strchr(beg, '.');
			if (nbeg)
			{
				*nbeg = 0;
				fieldVal = class_get_static_field(clazz, beg);
				*nbeg = '.';
				if (!fieldVal)
				{
					env->exception = exception_bad_variable_name;
					return 0;
				}
				nbeg++;
				return env_resolve_object_field(env, (object_t *)((value_t *)fieldVal->ovalue), nbeg, data, flags);
			}
			else
			{
				fieldVal = class_get_static_field(clazz, beg);
				if (!fieldVal)
				{
					env->exception = exception_bad_variable_name;
					return 0;
				}
				*data = &fieldVal->ovalue;
				*flags = fieldVal->flags;
				return 1;
			}
		}	
	}
	size_t valsize;

	// If we have a '[', we have an array we need to access
	beg = strchr(name, '[');
	if (beg)
	{
		*beg = 0;
		mapNode = map_find((map_t *)env->variables->data, name);
		if (!mapNode)
		{
			*beg = '[';
			env->exception = exception_bad_variable_name;
			return 0;
		}
		*beg = '[';

		char *num = beg + 1;
		char *numEnd = strchr(num, ']');
		luint index;
		if (!numEnd)
		{
			env->exception = exception_bad_variable_name;
			return 0;
		}

		*numEnd = 0;
		if (!is_numeric(numEnd))
		{
			*numEnd = ']';
			env->exception = exception_bad_array_index;
			return 0;
		}

		index = (luint)atoll(num);
		*numEnd = ']';

		array_t *arr = (array_t *)(((value_t *)mapNode->value)->ovalue);
		if (index >= arr->length)
		{
			env->exception = exception_bad_array_index;
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
		env->exception = exception_bad_variable_name;
		return 0;
	}

	value_t *val = (value_t *)mapNode->value;
	*flags = val->flags;
	*data = (data_t *)&val->ovalue;

	return 1;
}

int env_resolve_object_field(env_t *env, object_t *object, const char *name, data_t **data, flags_t *flags)
{
	if (!object)
	{
		env->exception = exception_null_dereference;
		return 0;
	}

	field_t *fieldData;
	char *beg = strchr(name, '.');
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
			env->exception = exception_bad_variable_name;
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
				env->exception = exception_bad_variable_name;
				return 0;
			}
			*data = (data_t *)&arr->length;
			*flags = 0;
			SET_TYPE(*flags, lb_uint);
			return 1;
			break;
		default:
			env->exception = exception_bad_variable_name;
			return 0;
			break;
		}
	}
}

int env_resolve_function_name(env_t *env, const char *name, function_t **function)
{
	if (!name)
	{
		env->exception = exception_function_not_found;
		return 0;
	}

	const char *funcname;
	function_t *result;
	char *end = strrchr(name, '.');
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
				env->exception = exception_function_not_found;
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
			funcname = end + 1;
			class_t *clazz = vm_load_class(env->vm, name);
			if (!clazz)
			{
				env->exception = exception_class_not_found;
				return 0;
			}
			*end = '.';
			result = class_get_function(clazz, funcname);
			if (!result)
			{
				env->exception = exception_function_not_found;
				return 0;
			}
			*function = result;
			return 1;
		}
	}
	else
	{
		class_t *clazz = *((class_t **)env->rbp + 2);
		result = class_get_function(clazz, name);
		if (!result)
		{
			env->exception = exception_function_not_found;
			return 0;
		}
		*function = result;
		return 1;
	}

	env->exception = exception_function_not_found;
	return 0;
}

int env_resolve_dynamic_function_name(env_t *env, const char *name, function_t **function, data_t **data, flags_t *flags)
{
	char *last = strrchr(name, '.');
	*last = 0;
	if (!env_resolve_variable(env, name, data, flags))
	{
		*last = '.';
		return 0;
	}
	*last = '.';

	byte_t type = TYPEOF(*flags);
	if (type != lb_object)
	{
		env->exception = exception_function_not_found;
		return 0;
	}

	object_t *object = (object_t *)(*data)->ovalue;
	if (!object)
	{
		env->exception = exception_null_dereference;
		return 0;
	}

	const char *funcName = last + 1;
	*function = class_get_function(object->clazz, funcName);
	if (!(*function))
	{
		env->exception = exception_function_not_found;
		return 0;
	}

	return 1;
}

int env_run_func_staticv(env_t *env, function_t *function, va_list ls)
{
	if (function->flags & FUNCTION_FLAG_NATIVE)
	{
		if (!function->location)
		{
			if (!try_link_function(env->vm, function))
				return env->exception = exception_link_error;
		}

		void *args = CALLOC(function->numargs + 2, sizeof(qword_t));

			//MALLOC((function->numargs * sizeof(qword_t)) + (2 * sizeof(qword_t)));
		if (!args)
			return env->exception = exception_vm_error;
		
		void **temp = (void **)args;
		temp[0] = env;
		temp[1] = function->parentClass;

		byte_t *types = (byte_t *)MALLOC(sizeof(byte_t) * function->numargs);
		if (!types)
		{
			FREE(args);
			return env->exception = exception_vm_error;
		}

		if (function->numargs > 0)
		{
			map_iterator_t *mit = map_create_iterator(function->argTypes);
			size_t i = 0;
			flags_t flags;
			while (mit->node)
			{
				switch ((byte_t)mit->value)
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
				mit = map_iterator_next(mit);
				i++;
			}
			map_iterator_free(mit);
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

		env->qret = vm_call_extern_asm(function->numargs + 2, NULL, args, function->location);

		FREE(types);
		FREE(args);

		return env->exception;
	}
	else
	{
		if (((char *)env->rsp) + (2 * sizeof(size_t)) > (char *)env->stack + env->vm->stackSize)
			return env->exception = exception_stack_overflow;

		*((size_t *)env->rsp) = (size_t)env->rbp;
		*((size_t *)env->rsp + 1) = (size_t)env->rip;
		*((size_t *)env->rsp + 2) = (size_t)function->parentClass;

		env->rbp = env->rsp;
		env->rsp = ((size_t *)env->rsp) + 3;

		env->variables->next = list_create();
		env->variables->next->prev = env->variables;
		env->variables = env->variables->next;
		env->variables->data = map_create(16, string_hash_func, string_compare_func, NULL, NULL, NULL);

		for (size_t i = 0; i < function->numargs; i++)
		{
			const char *argname = function->args[i];
			flags_t type = map_at(function->argTypes, argname);
			size_t size = sizeof_type(type);
			value_t val;
			val.flags = 0;
			value_set_type(&val, type);
			MEMCPY(&val.ovalue, ls, size);
			ls += size;

			void *loc;
			if (!(loc = stack_push(env, &val)))
				return env->exception;

			map_insert((map_t *)env->variables->data, argname, loc);
		}

		return env_run(env, function->location);
	}
}

int env_run_funcv(env_t *env, function_t *function, object_t *object, va_list ls)
{
	// push the arg list to the stack

	if (((char *)env->rsp) + (2 * sizeof(size_t)) > (char *)env->stack + env->vm->stackSize)
		return env->exception = exception_stack_overflow;

	*((size_t *)env->rsp) = (size_t)env->rbp;
	*((size_t *)env->rsp + 1) = (size_t)env->rip;
	*((size_t *)env->rsp + 2) = (size_t)function->parentClass;

	env->rbp = env->rsp;
	env->rsp = ((size_t *)env->rsp) + 3;

	env->variables->next = list_create();
	env->variables->next->prev = env->variables;
	env->variables = env->variables->next;
	env->variables->data = map_create(16, string_hash_func, string_compare_func, NULL, NULL, NULL);

	for (size_t i = 0; i < function->numargs; i++)
	{
		const char *argname = function->args[i];
		flags_t type = map_at(function->argTypes, argname);
		size_t size = sizeof_type(type);
		value_t val;
		val.flags = 0;
		value_set_type(&val, type);
		MEMCPY(&val.ovalue, ls, size);
		ls += size;

		void *loc;
		if (!(loc = stack_push(env, &val)))
			return env->exception;

		map_insert((map_t *)env->variables->data, argname, loc);
	}

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

	return env_run(env, function->location);
}

void env_free(env_t *env)
{
	FREE(env);
}

class_t *class_load_ext(const char *classname, vm_t *vm)
{
	return vm_load_class(vm, classname);
}

int env_run(env_t *env, void *location)
{
	env->rip = location;
	byte_t *counter = env->rip;	// A counter on where we are currently executing

	const char *name;			// An arbitrary string to store a name
	data_t *data;				// An arbitrary data_t pointer
	flags_t flags;				// An arbitrary flags_t
	const char *name2;			// An arbitrary string to store a name
	data_t *data2;				// An arbitrary data_t pointer
	flags_t flags2;				// An arbitrary flags_t
	value_t val;				// An arbitrary value
	value_t *valPtr;			// An arbitrary value pointer
	void *stackAllocLoc;		// A pointer to where a value on the stack was allocated
	function_t *callFunc;		// A pointer to a function which will be called
	function_t *callFunc2;		// A pointer to another function which will gbe called
	byte_t *callArgPtr;			// A pointer to some bytes which will be the function arguments
	byte_t *callFuncArgs;		// A pointer to some bytes which will be the function arguments
	size_t callFuncArgSize;		// The size of the call arguments
	list_iterator_t *lit;		// An arbitrary list iterator
	map_iterator_t *mip;		// An arbitrary map iterator
	byte_t callArgType;			// A byte to store the type of a call argument
	byte_t *cursor;				// A cursor into an array of bytes
	byte_t valueType;			// A byte to store the type of some value
	object_t *object;			// A pointer to an object_t used for holding some object
	object_t *object2;			// A pointer to another object_t used for holding some object
	class_t *clazz;				// A pointer to a class_t used for holding some class
	array_t *array;				// A pointer to an array_t used for holding some array
	size_t length;				// An arbitrary value for storing a length
	size_t off;					// An arbitrary value for storing an offset

	while (1)
	{
		switch (*counter)
		{
		case lb_noop:
			break;

		case lb_char:
			break;
		case lb_uchar:
			break;
		case lb_short:
			break;
		case lb_ushort:
			break;
		case lb_int:
			counter++;
			name = counter;
			if (!is_varname_avaliable(env, name))
				return env->exception = exception_bad_variable_name;
			counter += strlen(name) + 1;
			val.flags = 0;
			val.ivalue = 0;
			value_set_type(&val, lb_int);
			stackAllocLoc = stack_push(env, &val);
			if (!stackAllocLoc)
				return env->exception;
			map_insert((map_t *)env->variables->data, name, stackAllocLoc);
			break;
		case lb_uint:
			break;
		case lb_ulong:
			break;
		case lb_bool:
			break;
		case lb_float:
			break;
		case lb_object:
			counter++;
			name = counter;
			if (!is_varname_avaliable(env, name))
				return env->exception = exception_bad_variable_name;
			counter += strlen(name) + 1;
			val.flags = 0;
			val.ovalue = NULL;
			value_set_type(&val, lb_object);
			stackAllocLoc = stack_push(env, &val);
			if (!stackAllocLoc)
				return env->exception;
			map_insert((map_t *)env->variables->data, name, stackAllocLoc);
			break;
		case lb_chararray:
			break;
		case lb_uchararray:
			break;
		case lb_shortarray:
			break;
		case lb_ushortarray:
			break;
		case lb_intarray:
			break;
		case lb_uintarray:
			break;
		case lb_longarray:
			break;
		case lb_ulongarray:
			break;
		case lb_boolarray:
			break;
		case lb_floatarray:
			break;
		case lb_doublearray:
			break;
		case lb_objectarray:
			break;

		case lb_setb:
			counter++;
			name = counter;
			if (!env_resolve_variable(env, name, &data, &flags))
				return env->exception;
			counter += strlen(name) + 1;
			MEMCPY(data, counter, sizeof(byte_t));
			counter += sizeof(byte_t);
			break;
		case lb_setw:
			counter++;
			name = counter;
			if (!env_resolve_variable(env, name, &data, &flags))
				return env->exception;
			counter += strlen(name) + 1;
			MEMCPY(data, counter, sizeof(word_t));
			counter += sizeof(word_t);
			break;
		case lb_setd:
			counter++;
			name = counter;
			if (!env_resolve_variable(env, name, &data, &flags))
				return env->exception;
			counter += strlen(name) + 1;
			MEMCPY(data, counter, sizeof(dword_t));
			counter += sizeof(dword_t);
			break;
		case lb_setq:
			counter++;
			name = counter;
			if (!env_resolve_variable(env, name, &data, &flags))
				return env->exception;
			counter += strlen(name) + 1;
			MEMCPY(data, counter, sizeof(qword_t));
			counter += sizeof(qword_t);
			break;
		case lb_setr4:
			counter++;
			name = counter;
			if (!env_resolve_variable(env, name, &data, &flags))
				return env->exception;
			counter += strlen(name) + 1;
			MEMCPY(data, counter, sizeof(real4_t));
			counter += sizeof(real4_t);
			break;
		case lb_setr8:
			counter++;
			name = counter;
			if (!env_resolve_variable(env, name, &data, &flags))
				return env->exception;
			counter += strlen(name) + 1;
			MEMCPY(data, counter, sizeof(real8_t));
			counter += sizeof(real8_t);
			break;
		case lb_seto:
			counter++;
			name = (const char *)counter;
			if (!env_resolve_variable(env, name, &data, &flags))
				return env->exception;
			counter += strlen(name) + 1;
			switch (*counter)
			{
			case lb_new:
				counter++;
				name2 = (const char *)counter;
				clazz = vm_load_class(env->vm, name2); // This function will only load the class if it is not loaded
				if (!clazz)
					return env->exception = exception_class_not_found;
				counter += strlen(name2) + 1;
				callFunc = class_get_function(clazz, "<init>(");
				if (!callFunc)
					return env->exception = exception_function_not_found;
				counter += strlen((const char *)counter) + 1;
				object = manager_alloc_object(env->vm->manager, clazz);
				if (!object)
					return env->exception = exception_out_of_memory;
				if (env_run_func(env, callFunc, object))
					return env->exception;
				data->ovalue = object;
				break;
			case lb_value:
				counter++;
				name2 = (const char *)counter;
				if (!env_resolve_variable(env, name2, &data2, &flags2))
					return env->exception;
				counter += strlen(name2) + 1;
				data->ovalue = data2->ovalue;
				break;
			case lb_string:
				counter++;
				clazz = vm_get_class(env->vm, "String"); // Don't used vm_load_class here - String should have already been loaded
				if (!clazz)
					return env->exception = exception_class_not_found; // Throw exception if it is not loaded
				name2 = counter;
				callFunc = class_get_function(clazz, "<init>([C");
				if (!callFunc)
					return env->exception = exception_function_not_found;
				object = manager_alloc_object(env->vm->manager, clazz);
				if (!object)
					return env->exception = exception_out_of_memory;
				length = strlen(counter);
				array = manager_alloc_array(env->vm->manager, lb_chararray, length);
				if (!array)
					return env->exception = exception_out_of_memory;
				MEMCPY(&array->data, counter, length);
				counter += length + 1;
				if (env_run_func(env, callFunc, object, array))
					return env->exception;
				data->ovalue = object;
				break;
			case lb_null:
				data->ovalue = NULL;
				break;
			default:
				return env->exception = exception_bad_command;
				break;
			}
			break;
		case lb_setv:
			counter++;
			name = counter;
			if (!env_resolve_variable(env, name, &data, &flags))
				return env->exception;
			counter += strlen(name) + 1;
			name2 = counter;
			if (!env_resolve_variable(env, name2, &data2, &flags2))
				return env->exception;
			counter += strlen(name2) + 1;
			static_set(data, flags, data2, flags2);
			break;
		case lb_setr:
			counter++;
			name = counter;
			if (!env_resolve_variable(env, name, &data, &flags))
				return env->exception;
			counter += strlen(name) + 1;
			store_return(env, data, flags);
			break;

		case lb_ret:
			return env_cleanup_call(env);
			break;
		case lb_retb:
			counter++;
			env->bret = *(byte_t *)counter;
			return env_cleanup_call(env);
			break;
		case lb_retw:
			counter++;
			env->wret = *(word_t *)counter;
			return env_cleanup_call(env);
			break;
		case lb_retd:
			counter++;
			env->dret = *(dword_t *)counter;
			return env_cleanup_call(env);
			break;
		case lb_retq:
			counter++;
			env->qret = *(qword_t *)counter;
			return env_cleanup_call(env);
			break;
		case lb_retr4:
			counter++;
			env->r4ret = *(real4_t *)counter;
			return env_cleanup_call(env);
			break;
		case lb_retr8:
			counter++;
			env->r8ret = *(real8_t *)counter;
			return env_cleanup_call(env);
			break;
		case lb_reto:
			break;
		case lb_retv:
			counter++;
			name = counter;
			if (!env_resolve_variable(env, name, &data, &flags))
				return env->exception;
			MEMCPY(&env->vret, data, value_sizeof((value_t *)&flags));
			return env_cleanup_call(env);
			break;
		case lb_retr:
			return env_cleanup_call(env);
			break;

		case lb_static_call:
			counter++;
			name = counter;
			if (!env_resolve_function_name(env, name, &callFunc))
				return env->exception;
			counter += strlen(name) + 1;
			
			callFuncArgs = NULL;
			callFuncArgSize = callFunc->argSize;

			callFuncArgs = (byte_t *)MALLOC(callFuncArgSize);
			if (!callFuncArgs)
				return env->exception = exception_vm_error;
			cursor = callFuncArgs;

			callArgPtr = callFuncArgs;

			mip = map_create_iterator(callFunc->argTypes);
			while (mip->node)
			{
				callArgType = (flags_t)mip->value;

				switch (*counter)
				{
				case lb_byte:
					counter++;
					*cursor = *counter;
					cursor += 1;
					counter += 1;
					break;
				case lb_word:
					counter++;
					*((word_t *)cursor) = *((word_t *)counter);
					cursor += 2;
					counter += 2;
					break;
				case lb_dword:
					counter++;
					*((dword_t *)cursor) = *((dword_t *)counter);
					cursor += 4;
					counter += 4;
					break;
				case lb_qword:
					counter++;
					*((qword_t *)cursor) = *((qword_t *)counter);
					cursor += 8;
					counter += 8;
					break;
				case lb_string:
					counter++;
					clazz = vm_get_class(env->vm, "String"); // Don't used vm_load_class here - String should have already been loaded
					if (!clazz)
						return env->exception = exception_class_not_found; // Throw exception if it is not loaded
					name2 = counter;
					callFunc2 = class_get_function(clazz, "<init>([C");
					if (!callFunc2)
						return env->exception = exception_function_not_found;
					object2 = manager_alloc_object(env->vm->manager, clazz);
					if (!object2)
						return env->exception = exception_out_of_memory;
					length = strlen(counter);
					array = manager_alloc_array(env->vm->manager, lb_chararray, length);
					if (!array)
						return env->exception = exception_out_of_memory;
					MEMCPY(&array->data, counter, length);
					counter += length + 1;
					if (env_run_func(env, callFunc2, object2, array))
						return env->exception;
					*((qword_t *)cursor) = (qword_t)object2;
					cursor += 8;
					break;
				case lb_value:
					counter++;
					if (!env_resolve_variable(env, counter, &data, &flags))
					{
						FREE(callFuncArgs);
						return env->exception;
					}
					valueType = TYPEOF(flags);
					switch (valueType)
					{
					case lb_char:
					case lb_uchar:
						*cursor = data->cvalue;
						cursor += 1;
						//counter += 1;
						break;
					case lb_short:
					case lb_ushort:
						*((lshort *)cursor) = data->svalue;
						cursor += 2;
						//counter += 2;
						break;
					case lb_int:
					case lb_uint:
						*((lint *)cursor) = data->ivalue;
						cursor += 4;
						//counter += 4;
						break;
					case lb_long:
					case lb_ulong:
						*((llong *)cursor) = data->lvalue;
						cursor += 8;
						//counter += 8;
						break;
					case lb_bool:
						*((lbool *)cursor) = data->bvalue;
						cursor += 1;
						//counter += 1;
						break;
					case lb_float:
						*((lfloat *)cursor) = data->fvalue;
						cursor += 4;
						//counter += 4;
						break;
					case lb_double:
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
						*((lobject *)cursor) = data->ovalue;
						//counter += 8;
						cursor += 8;
						break;
					}
					counter += strlen(counter) + 1;
					break;
				default:
					FREE(callFuncArgs);
					return env->exception = exception_bad_command;
					break;
				}

				callArgPtr += sizeof_type(callArgType);
				mip = map_iterator_next(mip);
			}
			map_iterator_free(mip);

			env_run_func_staticv(env, callFunc, callFuncArgs);

			FREE(callFuncArgs);

			break;
		case lb_dynamic_call:
			counter++;
			name = counter;
			if (!env_resolve_dynamic_function_name(env, name, &callFunc, &data, &flags))
				return env->exception;
			counter += strlen(name) + 1;

			object = data->ovalue;

			callFuncArgs = NULL;
			callFuncArgSize = callFunc->argSize;

			callFuncArgs = (byte_t *)MALLOC(callFuncArgSize);
			if (!callFuncArgs)
				return env->exception = exception_vm_error;
			cursor = callFuncArgs;

			callArgPtr = callFuncArgs;

			mip = map_create_iterator(callFunc->argTypes);
			while (mip->node)
			{
				callArgType = (flags_t)mip->value;

				switch (*counter)
				{
				case lb_byte:
					counter++;
					*cursor = *counter;
					cursor += 1;
					counter += 1;
					break;
				case lb_word:
					counter++;
					*((word_t *)cursor) = *((word_t *)counter);
					cursor += 2;
					counter += 2;
					break;
				case lb_dword:
					counter++;
					*((dword_t *)cursor) = *((dword_t *)counter);
					cursor += 4;
					counter += 4;
					break;
				case lb_qword:
					counter++;
					*((qword_t *)cursor) = *((qword_t *)counter);
					cursor += 8;
					counter += 8;
					break;
				case lb_string:
					counter++;
					clazz = vm_get_class(env->vm, "String"); // Don't used vm_load_class here - String should have already been loaded
					if (!clazz)
						return env->exception = exception_class_not_found; // Throw exception if it is not loaded
					name2 = counter;
					callFunc2 = class_get_function(clazz, "<init>([C");
					if (!callFunc2)
						return env->exception = exception_function_not_found;
					object2 = manager_alloc_object(env->vm->manager, clazz);
					if (!object2)
						return env->exception = exception_out_of_memory;
					length = strlen(counter);
					array = manager_alloc_array(env->vm->manager, lb_chararray, length);
					if (!array)
						return env->exception = exception_out_of_memory;
					MEMCPY(&array->data, counter, length);
					counter += length + 1;
					if (env_run_func(env, callFunc2, object2, array))
						return env->exception;
					*((qword_t *)cursor) = (qword_t)object2;
					cursor += 8;
					break;
				case lb_value:
					counter++;
					if (!env_resolve_variable(env, counter, &data, &flags))
					{
						FREE(callFuncArgs);
						return env->exception;
					}
					valueType = TYPEOF(flags);
					switch (valueType)
					{
					case lb_char:
					case lb_uchar:
						*cursor = data->cvalue;
						cursor += 1;
						//counter += 1;
						break;
					case lb_short:
					case lb_ushort:
						*((lshort *)cursor) = data->svalue;
						cursor += 2;
						//counter += 2;
						break;
					case lb_int:
					case lb_uint:
						*((lint *)cursor) = data->ivalue;
						cursor += 4;
						//counter += 4;
						break;
					case lb_long:
					case lb_ulong:
						*((llong *)cursor) = data->lvalue;
						cursor += 8;
						//counter += 8;
						break;
					case lb_bool:
						*((lbool *)cursor) = data->bvalue;
						cursor += 1;
						//counter += 1;
						break;
					case lb_float:
						*((lfloat *)cursor) = data->fvalue;
						cursor += 4;
						//counter += 4;
						break;
					case lb_double:
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
						*((lobject *)cursor) = data->ovalue;
						cursor += 8;
						//counter += 8;
						break;
					}
					counter += strlen(counter) + 1;
					break;
				default:
					FREE(callFuncArgs);
					return env->exception = exception_bad_command;
					break;
				}

				callArgPtr += sizeof_type(callArgType);

				mip = map_iterator_next(mip);
			}
			map_iterator_free(mip);

			env_run_funcv(env, callFunc, object, callFuncArgs);

			FREE(callFuncArgs);
			break;

		case lb_add:
			counter++;
			if (!vmm_add(env, &counter))
				return env->exception;
			break;
		case lb_sub:
			counter++;
			if (!vmm_sub(env, &counter))
				return env->exception;
			break;
		case lb_mul:
			counter++;
			if (!vmm_mul(env, &counter))
				return env->exception;
			break;
		case lb_div:
			counter++;
			if (!vmm_div(env, &counter))
				return env->exception;
			break;
		case lb_mod:
			counter++;
			if (!vmm_mod(env, &counter))
				return env->exception;
			break;

		case lb_while:
			if (!vmc_compare(env, &counter))
			{
				if (env->exception)
					return env->exception;
				counter = CURR_CLASS(env)->data + *((size_t *)counter);
			}
			else
				counter += sizeof(size_t);
			break;

		case lb_if:
			if (handle_if(env, &counter))
				return env->exception;
			break;
		case lb_else:
		case lb_end:
			counter++;
			off = *((size_t *)counter);
			if (off == (size_t)-1)
			{
				counter += sizeof(size_t);
			}
			else
			{
				counter = CURR_CLASS(env)->data + off;
			}
			break;

		default:
			env->exception = exception_bad_command;
			env->exceptionDesc = *counter;
			return env->exception;
		}
	}
}

int env_cleanup_call(env_t *env)
{
	map_t *vars = (map_t *)env->variables->data;
	if (!vars)
		return env->exception = exception_illegal_state;

	map_free(vars, 0);

	// Remove the map from the list
	list_t *prev = env->variables->prev;
	env->variables->prev = NULL;
	list_free(env->variables, 0);
	env->variables = prev;
	env->variables->next = NULL;

	// Restore the fake registers from the previous call
	env->rsp = env->rbp;
	env->rbp = *((void **)env->rbp);
	env->rip = ((byte_t *)env->rbp) + sizeof(void *);

	return exception_none;
}

void *stack_push(env_t *env, value_t *value)
{
	byte_t *stack, *stackPtr;
	stack = (byte_t *)env->stack;
	stackPtr = (byte_t *)env->rsp;
	size_t size = value_sizeof(value) + sizeof(value->flags);
	byte_t *end = stackPtr + size;
	if (end - stack >= env->vm->stackSize)
	{
		env->exception = exception_stack_overflow;
		return NULL;
	}
	MEMCPY(stackPtr, value, size);
	env->rsp = end;
	return stackPtr;
}

int stack_pop(env_t *env, flags_t type)
{
	return exception_none;
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
		return cast_uchar(&src->cvalue, dstType, dst);
		break;
	case lb_short:
		return cast_short(&src->cvalue, dstType, dst);
		break;
	case lb_ushort:
		return cast_ushort(&src->cvalue, dstType, dst);
		break;
	case lb_int:
		return cast_int(&src->cvalue, dstType, dst);
		break;
	case lb_uint:
		return cast_uint(&src->cvalue, dstType, dst);
		break;
	case lb_long:
		return cast_long(&src->cvalue, dstType, dst);
		break;
	case lb_ulong:
		return cast_ulong(&src->cvalue, dstType, dst);
		break;
	case lb_bool:
		return cast_bool(&src->cvalue, dstType, dst);
		break;
	case lb_float:
		return cast_float(&src->cvalue, dstType, dst);
		break;
	case lb_double:
		return cast_double(&src->cvalue, dstType, dst);
		break;
	default:
		return 0;
	}
}

int try_link_function(vm_t *vm, function_t *func)
{
#if defined(_WIN32)
	char decName[512];
	sprintf_s(decName, sizeof(decName), "%s_%s", func->parentClass->name, func->name);
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

int equals_ignore_case(const char *s1, const char *s2)
{
	while (*s1 && *s2)
	{
		if (*s1 >= 64 && *s1 <= 90)
		{
			if (*s1 != *s2 && *s1 + 32 != *s2)
				return 0;
		}
		else if (*s1 >= 97 && *s1 <= 121)
		{
			if (*s1 != *s2 && *s1 - 32 != *s2)
				return 0;
		}
		else if (*s1 != *s2)
			return 0;

		s1++;
		s2++;
	}
	return *s1 == *s2;
}
