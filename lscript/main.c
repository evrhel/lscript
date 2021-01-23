#include "internal/vm.h"
#include "internal/mem_debug.h"

#include <stdio.h>
#include <memory.h>

#define HEAP_SIZE 64

#define KB_TO_B(KB) ((KB)*1024)
#define MB_TO_B(MB) (KB_TO_B((MB)*1024))
#define GB_TO_B(GB) (MB_TO_B((GB)*1024))

int main(int argc, char *argv[])
{
	BEGIN_DEBUG();

	char *vmArgs[] = { "-p", "..\\lsasm\\" };

	vm_t *vm = vm_create(KB_TO_B(4), 1024, sizeof(vmArgs) / sizeof(char **), vmArgs);

	class_t *clazz = vm_load_class(vm, "Main");
	if (!clazz)
	{
		printf("Failed to load Main class!\n");
		return 1;
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
	if (error = env_run_func_static(env, func, arr, NULL))
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