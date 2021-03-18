#include "class.h"

#include <stdio.h>
#include <string.h>
#include "mem_debug.h"

#define MAX_QUALIFIED_FUNCTION_NAME_LENGTH 1024
#define MAX_GLOBAL_VAR_NAME_LENGTH 1024

#define BUFLEN(ptr, start_arr) (sizeof(start_arr)-(ptr-start_arr))

static int register_functions(class_t *clazz, const byte_t *dataStart, const byte_t *dataEnd);
static int register_static_fields(class_t *clazz, const byte_t *dataStart, const byte_t *dataEnd);
static int register_field_offests(class_t *clazz, const byte_t *dataStart, const byte_t *dataEnd);

class_t *class_load(byte_t *binary, size_t length, classloadproc_t loadproc, void *more)
{
	class_t *result;

	if (length < 5)
		return NULL;

	result = (class_t *)CALLOC(1, sizeof(class_t));
	if (!result)
		return NULL;
	result->data = binary;

	byte_t *end = binary + length;
	byte_t *curr = binary;

	char compressed;
	unsigned int version;

	compressed = *curr; // Don't care about this right now
	curr++;

	version = *((unsigned int *)curr); // Will be read different on big-endian machines
	curr += sizeof(unsigned int);

	if (curr == end)
		return result;

	if (*curr != lb_class)
	{
		FREE(result);
		return NULL;
	}
	curr++;

	result->name = curr;
	curr += strlen(result->name) + 1;

	if (curr >= end)
	{
		if (!register_functions(result, curr, end))
		{
			FREE(result);
			return NULL;
		}
		if (!register_static_fields(result, curr, end))
		{
			map_free(result->functions, 0);
			FREE(result);
			return NULL;
		}
		if (!register_field_offests(result, curr, end))
		{
			map_free(result->functions, 0);
			map_free(result->staticFields, 0);
			FREE(result);
			return NULL;
		}
		return result;
	}

	if (*curr == lb_extends)
	{
		curr++;
		const char *superclassName = curr;
		if (!loadproc)
		{
			FREE(result);
			return NULL;
		}
		class_t *superclass = loadproc(superclassName, more);

		if (!superclass)
		{
			FREE(result);
			return NULL;
		}
		result->super = superclass;

		if (!result->functions)
			result->functions = map_create(CLASS_HASHTABLE_ENTRIES, string_hash_func, string_compare_func, string_copy_func, NULL, (free_func_t)free);
		map_iterator_t *mit = map_create_iterator(superclass->functions);
		while (mit->node)
		{
			function_t *func = (function_t *)mit->value;
			func->references++;
			map_insert(result->functions, mit->key, func);
			mit = map_iterator_next(mit);
		}
		map_iterator_free(mit);

		curr += strlen(superclassName) + 1;
	}

	if (!register_functions(result, curr, end))
	{
		FREE(result);
		return NULL;
	}
	if (!register_static_fields(result, curr, end))
	{
		map_free(result->functions, 0);
		FREE(result);
		return NULL;
	}
	if (!register_field_offests(result, curr, end))
	{
		map_free(result->functions, 0);
		map_free(result->staticFields, 0);
		FREE(result);
		return NULL;
	}
	return result;
}

void class_free(class_t *clazz, int freedata)
{
	map_iterator_t *it = map_create_iterator(clazz->functions);

	while (it->node)
	{	
		function_t *func = (function_t *)it->value;
		func->references--;
		if (func->references == 0)
		{
			map_t *argTypes = func->argTypes;

			FREE(func->args);

			map_iterator_t *ait = map_create_iterator(argTypes);
			while (ait->node)
			{
				ait->node->value = 0;
				ait = map_iterator_next(ait);
			}
			map_iterator_free(ait);

			map_free(argTypes, 1);

			FREE(it->key);
			FREE(func);
		}

		it = map_iterator_next(it);
	}
	map_iterator_free(it);

	map_free(clazz->functions, 0); // need to free each element individually here
	map_free(clazz->staticFields, 0);
	map_free(clazz->fields, 1);

	if (freedata)
		FREE((byte_t *)clazz->data);

	FREE(clazz);
}

int register_functions(class_t *clazz, const byte_t *dataStart, const byte_t *dataEnd)
{
	if (!clazz->functions)
		clazz->functions = map_create(CLASS_HASHTABLE_ENTRIES, string_hash_func, string_compare_func, string_copy_func, NULL, (free_func_t)free);
	char qualifiedName[MAX_QUALIFIED_FUNCTION_NAME_LENGTH];
	char *qualnamePtr;

	const char *funcName;
	unsigned char numArgs;
	unsigned char argtype;
	const char *argname;
	unsigned char isStatic;
	unsigned char isNative;

	map_t *argTypes;
	function_t *func;
	list_t *argorder;
	list_t *argorderLast;
	list_iterator_t *it;
	size_t i;
	size_t argSize;

	const byte_t *curr = dataStart;
	while (curr < dataEnd)
	{
		switch (*curr)
		{
		case lb_function:
			curr++;
			qualnamePtr = qualifiedName;
			if (*curr == lb_static)
				isStatic = 1;
			else if (*curr == lb_dynamic)
				isStatic = 0;
			else
			{
				map_free(clazz->functions, 0);
				return 0;
			}
			curr++;

			if (*curr == lb_interp)
				isNative = 0;
			else if (*curr == lb_native)
				isNative = 1;
			else
			{
				map_free(clazz->functions, 0);
				return 0;
			}
			curr++;

			funcName = curr;
			sprintf_s(qualnamePtr, MAX_QUALIFIED_FUNCTION_NAME_LENGTH, "%s(", funcName);
			qualnamePtr += strlen(qualnamePtr);

			curr += (qualnamePtr - qualifiedName);
			numArgs = *curr;
			curr++;

			argTypes = map_create(4, string_hash_func, string_compare_func, string_copy_func, NULL, (free_func_t)free);
			argorder = list_create();
			argorderLast = argorder;

			argSize = 0;

			for (unsigned char i = 0; i < numArgs; i++)
			{
				argtype = *curr;
				curr++;
				switch (argtype)
				{
				case lb_chararray:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "[");
					qualnamePtr++;
					argSize += sizeof(lchararray) - sizeof(lchar);
				case lb_char:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "C");
					qualnamePtr++;
					argSize += sizeof(lchar);
					break;

				case lb_uchararray:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "[");
					qualnamePtr++;
					argSize += sizeof(luchararray) - sizeof(luchar);
				case lb_uchar:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "c");
					qualnamePtr++;
					argSize += sizeof(luchar);
					break;

				case lb_shortarray:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "[");
					qualnamePtr++;
					argSize += sizeof(lshortarray) - sizeof(lshort);
				case lb_short:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "S");
					qualnamePtr++;
					argSize += sizeof(lshort);
					break;

				case lb_ushortarray:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "[");
					qualnamePtr++;
					argSize += sizeof(lushortarray) - sizeof(lushort);
				case lb_ushort:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "s");
					qualnamePtr++;
					argSize += sizeof(lushort);
					break;

				case lb_intarray:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "[");
					qualnamePtr++;
					argSize += sizeof(lintarray) - sizeof(lint);
				case lb_int:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "I");
					qualnamePtr++;
					argSize += sizeof(lint);
					break;

				case lb_uintarray:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "[");
					qualnamePtr++;
					argSize += sizeof(luintarray) - sizeof(luint);
				case lb_uint:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "i");
					qualnamePtr++;
					argSize += sizeof(luint);
					break;

				case lb_longarray:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "[");
					qualnamePtr++;
					argSize += sizeof(llongarray) - sizeof(llong);
				case lb_long:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "Q");
					qualnamePtr++;
					argSize += sizeof(llong);
					break;

				case lb_ulongarray:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "[");
					qualnamePtr++;
					argSize += sizeof(lulongarray) - sizeof(lulong);
				case lb_ulong:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "q");
					qualnamePtr++;
					argSize += sizeof(lulong);
					break;

				case lb_boolarray:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "[");
					qualnamePtr++;
					argSize += sizeof(lboolarray) - sizeof(lbool);
				case lb_bool:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "B");
					qualnamePtr++;
					argSize += sizeof(lbool);
					break;

				case lb_floatarray:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "[");
					qualnamePtr++;
					argSize += sizeof(lfloatarray) - sizeof(lfloat);
				case lb_float:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "F");
					qualnamePtr++;
					argSize += sizeof(lfloat);
					break;

				case lb_doublearray:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "[");
					qualnamePtr++;
					argSize += sizeof(ldoublearray) - sizeof(ldouble);
				case lb_double:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "D");
					qualnamePtr++;
					argSize += sizeof(ldouble);
					break;

				case lb_objectarray:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "[");
					qualnamePtr++;
					argSize += sizeof(lobjectarray) - sizeof(lobject);
				case lb_object:
					sprintf_s(qualnamePtr, BUFLEN(qualnamePtr, qualifiedName), "L%s;", curr);
					qualnamePtr += strlen(qualnamePtr);
					curr += strlen(curr) + 1;
					argSize += sizeof(lobject);
					break;
				}
				argname = curr;
				map_insert(argTypes, argname, (void *)argtype);
				argorderLast->next = list_create();
				argorderLast->next->prev = argorderLast;
				argorderLast = argorderLast->next;
				argorderLast->data = (void *)argname;

				curr += strlen(argname) + 1;
			}

			func = (function_t *)MALLOC(sizeof(function_t));
			if (func)
			{
				func->name = funcName;
				func->location = isNative ? NULL : (void *)curr;
				func->argTypes = argTypes;
				func->numargs = numArgs;
				func->parentClass = clazz;
				func->flags = 0;
				func->argSize = argSize;
				func->references = 1;

				if (isStatic)
					func->flags |= FUNCTION_FLAG_STATIC;

				if (isNative)
					func->flags |= FUNCTION_FLAG_NATIVE;

				func->args = MALLOC(numArgs * sizeof(const char *));
				if (!func->args)
				{
					// Memory leak here, need to free stuff
					return 0;
				}

				it = list_create_iterator(argorder->next);
				for (i = 0; i < numArgs; i++)
				{
					func->args[i] = (const char *)it->data;
					it = list_iterator_next(it);
				}
				list_iterator_free(it);

				//func->argOrder = argorder->next;

				map_insert(clazz->functions, qualifiedName, func);

				if (argorder->next)
				{
					argorder->next->prev = NULL;
					argorder->next = NULL;
				}
				list_free(argorder, 0);
			}
			break;
		case lb_setb:
		case lb_retb:
			curr += 2;
			break;
		case lb_setw:
		case lb_retw:
			curr += 3;
			break;
		case lb_setd:
		case lb_retd:
			curr += 5;
			break;
		case lb_setq:
		case lb_retq:
			curr += 9;
			break;
		case lb_setr4:
		case lb_retr4:
			curr += 5;
			break;
		case lb_setr8:
		case lb_retr8:
			curr += 9;
			break;
		case lb_add:
		case lb_sub:
		case lb_mul:
		case lb_div:
		case lb_mod:
			curr++;
			curr += strlen(curr) + 1;
			curr += strlen(curr) + 1;
			if (*curr == lb_value)
			{
				curr += strlen(curr) + 2;
			}
			else
				curr += sizeof_type(*curr) + 1;
			break;
			
		default:
			curr++;
			break;
		}
	}
	return 1;
}

int register_static_fields(class_t *clazz, const byte_t *dataStart, const byte_t *dataEnd)
{
	if (!clazz->staticFields)
		clazz->staticFields = map_create(CLASS_HASHTABLE_ENTRIES, string_hash_func, string_compare_func, string_copy_func, NULL, (free_func_t)free);
	const char *globalName;

	const char *curr = dataStart;
	while (curr < dataEnd)
	{
		switch (*curr)
		{
		case lb_global:
			curr++;
			globalName = curr;
			curr += strlen(globalName) + 1;
			if (*curr == lb_static)
			{
				map_insert(clazz->staticFields, globalName, curr);
				value_t *val = (value_t *)curr;
				curr += 8 + value_sizeof(val);
			}
			break;
		default:
			curr++;
			break;
		}
	}
	return 1;
}

int register_field_offests(class_t *clazz, const byte_t *dataStart, const byte_t *dataEnd)
{
	if (!clazz->fields)
		clazz->fields = map_create(CLASS_HASHTABLE_ENTRIES, string_hash_func, string_compare_func, string_copy_func, NULL, (free_func_t)free);
	const char *fieldName;

	size_t valueSize;
	size_t currentOffset = 0;

	const byte_t *curr = dataStart;
	while (curr < dataEnd)
	{
		switch (*curr)
		{
		case lb_global:
			curr++;
			fieldName = curr;
			curr += strlen(fieldName) + 1;
			valueSize = sizeof_type(*(curr + VALUE_TYPE_OFFSET));
			if (*curr == lb_dynamic || *curr == 0)
			{
				field_t *field = (field_t *)MALLOC(sizeof(field_t));
				if (!field)
				{
					map_free(clazz->fields, 1);
					clazz->fields = NULL;
					return 0;
				}
				field->flags = *((flags_t *)curr);
				field->offset = (void *)currentOffset;
				map_insert(clazz->fields, fieldName, field);
				currentOffset += valueSize;
				curr += 8;
			}
			else
			{
				curr += 8 + valueSize;
			}
			break;
		default:
			curr++;
			break;
		}
	}

	clazz->size = currentOffset;
	return 1;
}
