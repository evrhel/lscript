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

class_t *class_load(byte_t *binary, size_t length, int loadSuperclasses, classloadproc_t loadproc, void *more)
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
	size_t len;
	curr += (len = (strlen(result->name) + 1));
	result->safeName = (char *)MALLOC(len);
	if (!result->safeName)
	{
		FREE(result);
		return NULL;
	}
	strcpy_s(result->safeName, len, result->name);
	char *cursor = result->safeName;
	while (*cursor)
	{
		if (*cursor == '.') *cursor = '_';
		cursor++;
	}

	result->debug = NULL;

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

	char *superclassName = NULL;
	if (*curr == lb_extends)
	{
		curr++;
		superclassName = (char *)curr;

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

	if (loadSuperclasses)
	{
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

		set_superclass(result, superclass);
	}

	return result;
}

int set_superclass(class_t *__restrict clazz, class_t *__restrict superclass)
{
	if (clazz->super)
		return 0;

	clazz->super = superclass;

	if (!clazz->functions)
		clazz->functions = map_create(CLASS_HASHTABLE_ENTRIES, string_hash_func, string_compare_func, string_copy_func, NULL, (free_func_t)free);
	map_iterator_t *mit = map_create_iterator(superclass->functions);
	while (mit->node)
	{
		function_t *func = (function_t *)mit->value;
		func->references++;

		// Make sure we don't overwrite any overriden functions
		function_t *implemented = (function_t *)map_at(clazz->functions, func->qualifiedName);
		if (!implemented)
		{
			func->references++;
			map_insert(clazz->functions, mit->key, func);
		}

		mit = map_iterator_next(mit);
	}
	map_iterator_free(mit);

	return 1;
}

void class_free(class_t *__restrict clazz, int freedata)
{
	map_iterator_t *it = map_create_iterator(clazz->functions);

	//__check_native_corruption();
	while (it->node)
	{	
		function_t *func = (function_t *)it->value;
		func->references--;
		if (func->references == 0)
		{
			FREE(func->qualifiedName);
			func->qualifiedName = NULL;

			map_t *argTypes = func->argTypes;

			FREE(func->args);
			func->args = NULL;

			map_iterator_t *ait = map_create_iterator(argTypes);
			while (ait->node)
			{
				ait->node->value = 0;
				ait = map_iterator_next(ait);
			}
			map_iterator_free(ait);

			map_free(argTypes, 1);

			FREE(it->key);
			it->key = NULL;

			
			//__dfree(func, "class.c", 205);
			FREE(func);
			it->value = NULL;
		}

		it = map_iterator_next(it);
	}
	map_iterator_free(it);

	map_free(clazz->functions, 0); // need to free each element individually here
	map_free(clazz->staticFields, 0);
	map_free(clazz->fields, 1);

	if (clazz->debug)
		free_debug(clazz->debug);

	if (freedata)
	{
		FREE((byte_t *)clazz->data);
		clazz->data = NULL;
	}

	if (clazz->safeName)
	{
		FREE(clazz->safeName);
		clazz->safeName = NULL;
	}

	FREE(clazz);
}

int register_functions(class_t *clazz, const byte_t *dataStart, const byte_t *dataEnd)
{
	if (!clazz->functions)
		clazz->functions = map_create(CLASS_HASHTABLE_ENTRIES, string_hash_func, string_compare_func, string_copy_func, NULL, (free_func_t)free);
	char qualifiedName[MAX_QUALIFIED_FUNCTION_NAME_LENGTH];
	char *qualnamePtr;
	size_t qualnameSize;

	const char *funcName;
	unsigned char numArgs;
	unsigned char argtype;
	const char *argname;
	unsigned char isStatic;
	byte_t execType;
	byte_t returnType;

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

			execType = *curr;
			if (execType < lb_interp || execType > lb_abstract)
			{
				map_free(clazz->functions, 0);
				return 0;
			}
			curr++;

			if (*curr < lb_void || *curr > lb_objectarray)
			{
				map_free(clazz->functions, 0);
				return 0;
			}
			returnType = *curr;
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
				qualnameSize = strlen(qualifiedName) + 1;
				func->qualifiedName = (char *)MALLOC(qualnameSize);
				if (!func->qualifiedName)
					break;
				strcpy_s(func->qualifiedName, qualnameSize, qualifiedName);

				func->name = funcName;
				func->location = execType == lb_interp ? (void *)curr : NULL;
				func->argTypes = argTypes;
				func->numargs = numArgs;
				func->parentClass = clazz;
				func->flags = 0;
				func->argSize = argSize;
				func->references = 1;
				func->returnType = returnType;

				if (isStatic)
					func->flags |= FUNCTION_FLAG_STATIC;

				if (execType == lb_native)
					func->flags |= FUNCTION_FLAG_NATIVE;
				else if (execType == lb_abstract)
					func->flags |= FUNCTION_FLAG_ABSTRACT;

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
		case lb_and:
		case lb_or:
		case lb_xor:
		case lb_lsh:
		case lb_rsh:
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
		case lb_neg:
		case lb_not:
			curr++;
			curr += strlen(curr) + 1;
			curr += strlen(curr) + 1;
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

const char *class_get_last_error()
{
	return NULL;
}
