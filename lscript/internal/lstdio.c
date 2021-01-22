#include "lstdio.h"

#include <stdio.h>

#include "vm.h"
#include "mem_debug.h"

LNIFUNC lulong LNICALL ls_open(LEnv venv, lclass vclazz, lobject filepath, lint mode)
{
	env_t *env = (env_t *)venv;
	if (!filepath)
	{
		env->exception = exception_null_dereference;
		return NULL;
	}

	array_t *arr = (array_t *)object_get_object((object_t *)filepath, "chars");
	char *data = (char *)&arr->data;
	FILE *handle;
	const char *modestr;
	switch (mode)
	{
	case 0:
		modestr = "r";
		break;
	case 1:
		modestr = "rb";
		break;
	case 2:
		modestr = "w";
		break;
	case 3:
		modestr = "wb";
		break;
	default:
		env->exception = exception_illegal_state;
		return NULL;
		break;
	}

	fopen_s(&handle, data, modestr);

	return (lulong)handle;
}

LNIFUNC void LNICALL ls_close(LEnv venv, lclass vclazz, lulong handle)
{
	fclose((FILE *)handle);
}

LNIFUNC void LNICALL ls_putc(LEnv venv, lclass vclazz, lulong handle, lchar c)
{
	fputc(c, (FILE *)handle);
}

LNIFUNC luint LNICALL ls_write(LEnv venv, lclass vclazz, lulong handle, lchararray data, luint off, luint length)
{

	luint temp = off + length;
	env_t *env = (env_t *)venv;
	if (!data)
	{
		env->exception = exception_null_dereference;
		return 0;
	}

	array_t *arr = (array_t *)data;
	char *cdata = (char *)&arr->data;
	luint result = (luint)fwrite(cdata, sizeof(char), arr->length, /*(FILE *)handle*/stdout);
	return result;
}

LNIFUNC void LNICALL putls(LEnv venv, lclass vclazz, lobject string)
{
	env_t *env = (env_t *)venv;
	if (!string)
	{
		env->exception = exception_null_dereference;
		return;
	}

	array_t *arr = (array_t *)object_get_object((object_t *)string, "chars");
	char *data = (char *)&arr->data;
	for (luint i = 0; i < arr->length; i++)
		putchar(data[i]);
}
