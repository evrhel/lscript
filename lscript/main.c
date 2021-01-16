#include "internal/vm.h"
#include "internal/mem_debug.h"

#include <stdio.h>
#include <memory.h>

#define HEAP_SIZE 64

byte_t string_class[] =
{
	0x01,
	0x01, 0x00, 0x00, 0x00,

	lb_class, 'S', 't', 'r', 'i', 'n', 'g', 0x00,
		lb_global, 'd', 'a', 't', 'a', 0x00, lb_dynamic, lb_varying, 0x00, 0x00, 0x00, 0x00, 0x00, lb_chararray
};

byte_t data[] =
{
	// Header info
	0x01,
	0x01, 0x00, 0x00, 0x00,

	// Code
	lb_class, 'M', 'a', 'i', 'n', 0x00,
		lb_global, 'm', 'y', 'N', 'u', 'm', 0x00, lb_static, lb_const, 0x00, 0x00, 0x00, 0x00, 0x00, lb_uint, 0x16, 0x00, 0x00, 0x00,
		lb_global, 'm', 'y', 'N', 'u', 'm', '2', 0x00, lb_static, lb_varying, 0x00, 0x00, 0x00, 0x00, 0x00, lb_uint, 0x13, 0x00, 0x00, 0x00,

		lb_global, 'm', 'y', 'F', 'i', 'e', 'l', 'd', 0x00, lb_dynamic, lb_varying, 0x00, 0x00, 0x00, 0x00, 0x00, lb_int,
		lb_global, 'm', 'y', 'F', 'i', 'e', 'l', 'd', '2', 0x00, lb_dynamic, lb_varying, 0x00, 0x00, 0x00, 0x00, 0x00, lb_object,
		lb_global, 'a', 'r', 'r', 'a', 'y', 0x00, lb_dynamic, lb_varying, 0x00, 0x00, 0x00, 0x00, 0x00, lb_intarray,

		lb_function, lb_static, lb_interp, 'm', 'a', 'i', 'n', 0x00, 0x02, lb_intarray, 'a', 'r', 'g', 's', 0x00, lb_object, 'S', 't', 'r', 'i', 'n', 'g', 0x00, 's', 't', 'r', 0x00,

			lb_static_call, '_', 'p', 'u', 't', 's', '(', 'L', 'S', 't', 'r', 'i', 'n', 'g', ';', 0x00, lb_value, 's', 't', 'r', 0x00,

			lb_int, 'v', 'a', 'r', 'i', 'a', 'b', 'l', 'e', 0x00,
			lb_setl, 'v', 'a', 'r', 'i', 'a', 'b', 'l', 'e', 0x00, 0x0a, 0x00, 0x00, 0x00,

			lb_object, 'm', 'y', 'O', 'b', 'j', 0x00,
			lb_seto, 'm', 'y', 'O', 'b', 'j', 0x00, lb_new, 'M', 'a', 'i', 'n', 0x00,
			lb_dynamic_call, 'm', 'y', 'O', 'b', 'j', '.', 'd', 'F', 'u', 'n', 'c', '(', 0x00,
			lb_setr, 'a', 'r', 'g', 's', '[', '3', ']', 0x00,

			lb_static_call, 'm', 'y', 'F', 'u', 'n', 'c', '(', 0x00,
			lb_setr, 'a', 'r', 'g', 's', '[', '0', ']', 0x00,

			lb_setv, 'v', 'a', 'r', 'i', 'a', 'b', 'l', 'e', 0x00, 'a', 'r', 'g', 's', '[', '0', ']', 0x00,

			lb_static_call, 't', 'e', 's', 't', 'F', 'u', 'n', 'c', '(', 0x00,
			lb_setr, 'a', 'r', 'g', 's', '[', '1', ']', 0x00,

			lb_add, 'v', 'a', 'r', 'i', 'a', 'b', 'l', 'e', 0x00, 'v', 'a', 'r', 'i', 'a', 'b', 'l', 'e', 0x00, lb_int, 0x10, 0x00, 0x00, 0x00,
			lb_retv, 'v', 'a', 'r', 'i', 'a', 'b', 'l', 'e', 0x00,

		lb_function, lb_static, lb_interp, 'm', 'y', 'F', 'u', 'n', 'c', 0x00, 0x00,
			lb_retl, 0x11, 0x00, 0x00, 0x00,

		lb_function, lb_dynamic, lb_interp, 'd', 'F', 'u', 'n', 'c', 0x00, 0x00,
			lb_setl, 't', 'h', 'i', 's', '.', 'm', 'y', 'F', 'i', 'e', 'l', 'd', 0x00, 0x20, 0x00, 0x00, 0x00,
			lb_retv, 't', 'h', 'i', 's', '.', 'm', 'y', 'F', 'i', 'e', 'l', 'd', 0x00,

		lb_function, lb_static, lb_native, 't', 'e', 's', 't', 'F', 'u', 'n', 'c', 0x00, 0x00,
		lb_function, lb_static, lb_native, '_', 'p', 'u', 't', 's', 0x00, 0x01, lb_object, 'S', 't', 'r', 'i', 'n', 'g', 0x00, 's', 't', 'r', 0x00
};

#define KB_TO_B(KB) ((KB)*1024)
#define MB_TO_B(MB) (KB_TO_B((MB)*1024))
#define GB_TO_B(GB) (MB_TO_B((GB)*1024))

int main(int argc, char *argv[])
{
	BEGIN_DEBUG();



	heap_t *hp = create_heap(KB_TO_B(4));
	void *first, *second, *third, *fourth;
	first = halloc(hp, 16);
	memset(first, 0x66, 16);
	second = halloc(hp, 20);
	memset(second, 0x77, 20);
	third = halloc(hp, 30);
	memset(third, 0x88, 30);
	fourth = halloc(hp, 12);
	memset(fourth, 0x99, 12);


	vm_t *vm = vm_create(KB_TO_B(4), 512);

	byte_t *stringHeapData = MALLOC(sizeof(string_class));
	if (!stringHeapData)
	{
		vm_free(vm);
		return 1;
	}

	MEMCPY(stringHeapData, string_class, sizeof(string_class));

	class_t *stringClass = vm_load_class_binary(vm, stringHeapData, sizeof(string_class));
	if (!stringClass)
	{
		printf("Failed to load string class!\n");
		return 2;
	}
	object_t *object = manager_alloc_object(vm->manager, stringClass);
	array_t *array = manager_alloc_array(vm->manager, lb_chararray, 5);
	MEMCPY(&array->data, "hello", array->length);
	object_set_object(object, "data", array);

	byte_t *heapData = MALLOC(sizeof(data));
	if (!heapData)
	{
		vm_free(vm);
		return 1;
	}

	MEMCPY(heapData, data, sizeof(data));

	class_t *clazz = vm_load_class_binary(vm, heapData, sizeof(data));
	if (!clazz)
	{
		printf("Failed to load class!\n");
		return 2;
	}

	env_t *env = env_create(vm);

	function_t *func = class_get_function(clazz, "main([ILString;");
	function_t *func1 = class_get_function(clazz, "myFunc(");

	array_t *arr = manager_alloc_array(vm->manager, lb_intarray, 4);
	array_set_int(arr, 0, 5);
	array_set_int(arr, 1, 10);
	array_set_int(arr, 2, 25);
	array_set_int(arr, 3, 20);

	int error;
	if (error = env_run_func_static(env, func, arr, object))
	{
		printf("Exception thrown: %d\n", error);
	}
	else
	{
		printf("Function returned: %d\n", env->dret);
	}

	for (int i = 0; i < arr->length; i++)
		printf("array[%d] = %d\n", i,  array_get_int(arr, i));

	vm_free(vm);

	END_DEBUG();

	return 0;
}