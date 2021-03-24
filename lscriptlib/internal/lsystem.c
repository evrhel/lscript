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
	return NULL;
}

LNIFUNC lobject LNICALL System_setProperty(LEnv venv, lclass vclazz, lobject lspropname, lobject lspropvalue)
{
	return NULL;
}

LNIFUNC lobject LNICALL System_arraycopy(LEnv venv, lclass vclazz, lobject dst, luint dstOff, lobject src, luint srcOff, luint len)
{
	env_t *env = (env_t *)venv;
	class_t *clazz = (class_t *)vclazz;

	array_t *dstarr = (array_t *)dst;
	array_t *srcarr = (array_t *)src;

	byte_t dsttype = value_typeof((value_t *)dst);
	byte_t srctype = value_typeof((value_t *)src);

	if (dsttype <= lb_object || srctype <= lb_object)
	{
		env_raise_exception(env, exception_illegal_state, "Non-array passed into arraycopy");
		return NULL;
	}

	if (dsttype != srctype)
	{
		env_raise_exception(env, exception_illegal_state, "arraycopy type mismatch");
		return NULL;
	}

	if (dstOff + len > dstarr->length || srcOff + len > srcarr->length)
	{
		env_raise_exception(env, exception_bad_array_index, "Copy out of array bounds");
		return NULL;
	}

	dsttype -= 12;
	srctype = dsttype;

	size_t elemsize = sizeof_type(dsttype);
	size_t copylen = elemsize * len;

	byte_t *dstdata = (byte_t *)&dstarr->data;
	byte_t *srcdata = (byte_t *)&srcarr->data;
	memcpy(dstdata + (elemsize * dstOff), srcdata + (elemsize * srcOff), copylen);

	return dst;
}
