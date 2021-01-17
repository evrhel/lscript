#include "lstdio.h"

#include <stdio.h>

#include "vm.h"
#include "mem_debug.h"

LNIFUNC lint LNICALL testFunc(LEnv env, lclass clazz)
{
	printf("From testFunc\n");

	return 67;
}

LNIFUNC void LNICALL putls(LEnv venv, lclass vclazz, lobject string)
{
	env_t *env = (env_t *)venv;
	if (!string)
	{
		env->exception = exception_null_dereference;
		return;
	}

	array_t *arr = (array_t *)object_get_object(string, "data");
	char *data = (char *)&arr->data;
	for (luint i = 0; i < arr->length; i++)
		putchar(data[i]);
}
