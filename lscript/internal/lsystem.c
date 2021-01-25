#include "lsystem.h"

#include "vm.h"

LNIFUNC void LNICALL System_exit(LEnv venv, lclass vclazz, lint code)
{
	exit(code);
}

LNIFUNC void LNICALL System_loadLibrary(LEnv venv, lclass vclazz, lobject lslibname)
{
	env_t *env = (env_t *)venv;
	vm_t *vm = env->vm;

	object_t *obj = (object_t *)lslibname;
	if (!obj)
	{
		env->exception = exception_null_dereference;
		return;
	}

	array_t *arr = (array_t *)object_get_object(obj, "chars");
	char *buf = (char *)malloc((size_t)arr->length + 1);
	if (!buf)
	{
		env->exception = exception_out_of_memory;
		return;
	}
	buf[arr->length] = 0;
	memcpy(buf, &arr->data, arr->length);

	int result = vm_load_library(vm, buf);

	free(buf);
}

LNIFUNC lobject LNICALL System_getProperty(LEnv venv, lclass vclazz, lobject lspropname)
{

}

LNIFUNC lobject LNICALL System_setProperty(LEnv venv, lclass vclazz, lobject lspropname, lobject lspropvalue)
{

}
