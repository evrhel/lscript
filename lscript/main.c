#include "internal/vm.h"
#include "internal/mem_debug.h"

#include <stdio.h>
#include <memory.h>

#define HEAP_SIZE 64

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

		lb_function, lb_static, 'm', 'a', 'i', 'n', 0x00, 0x01, lb_intarray, 'a', 'r', 'g', 's', 0x00,
			lb_int, 'v', 'a', 'r', 'i', 'a', 'b', 'l', 'e', 0x00,
			lb_setl, 'v', 'a', 'r', 'i', 'a', 'b', 'l', 'e', 0x00, 0x0a, 0x00, 0x00, 0x00,
			lb_static_call, 'm', 'y', 'F', 'u', 'n', 'c', '(', 0x00,
			lb_setr, 'a', 'r', 'g', 's', '[', '0', ']', 0x00,
			lb_setv, 'v', 'a', 'r', 'i', 'a', 'b', 'l', 'e', 0x00, 'a', 'r', 'g', 's', '[', '0', ']', 0x00,
			lb_add, 'v', 'a', 'r', 'i', 'a', 'b', 'l', 'e', 0x00, 'v', 'a', 'r', 'i', 'a', 'b', 'l', 'e', 0x00, lb_int, 0x02, 0x00, 0x00, 0x00,
			lb_retv, 'v', 'a', 'r', 'i', 'a', 'b', 'l', 'e', 0x00,

		lb_function, lb_static, 'm', 'y', 'F', 'u', 'n', 'c', 0x00, 0x00,
			lb_retl, 0x11, 0x00, 0x00, 0x00
};

int main(int argc, char *argv[])
{
	BEGIN_DEBUG();

	vm_t *vm = vm_create(1024, 512);

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

	function_t *func = class_get_function(clazz, "main([I");
	function_t *func1 = class_get_function(clazz, "myFunc(");

	array_t *arr = manager_alloc_array(vm->manager, lb_intarray, 4);
	array_set_int(arr, 0, 5);
	array_set_int(arr, 1, 10);
	array_set_int(arr, 2, 25);
	array_set_int(arr, 3, 20);

	int error;
	if (error = env_run_func_static(env, func, arr))
	{
		printf("Exception thrown: %d\n", error);
	}
	else
	{
		printf("Function returned: %d\n", env->lret);
	}

	vm_free(vm);

	END_DEBUG();

	return 0;
}