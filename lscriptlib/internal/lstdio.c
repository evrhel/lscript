#include "lstdio.h"

#include <stdio.h>

#include "vm.h"
#include "mem_debug.h"

LNIFUNC lulong LNICALL lscript_io_StdFileHandle_fopen(LEnv venv, lclass vclazz, lobject filepath, lint mode)
{
	env_t *env = (env_t *)venv;
	if (!filepath)
	{
		env_raise_exception(env, exception_null_dereference, "filepath");
		return 0;
	}

	char pathstr[MAX_PATH];

	array_t *arr = (array_t *)object_get_object((object_t *)filepath, "chars");
	if (arr->length > MAX_PATH - 1)
	{
		env_raise_exception(env, exception_illegal_state, "filepath too long");
		return 0;
	}
	
	char *data = (char *)&arr->data;

	memcpy(pathstr, data, arr->length);
	pathstr[arr->length] = 0;

	FILE *handle;
	const char *modestr;
	switch (mode)
	{
	case 1:
		modestr = "r";
		break;
	case 2:
		modestr = "rb";
		break;
	case 3:
		modestr = "w";
		break;
	case 4:
		modestr = "wb";
		break;
	default:
		env_raise_exception(env, exception_illegal_state, "Illegal file mode %d", mode);
		return 0;
		break;
	}

	fopen_s(&handle, pathstr, modestr);

	return (lulong)handle;
}

LNIFUNC void LNICALL lscript_io_StdFileHandle_fclose(LEnv venv, lclass vclazz, lulong handle)
{
	fclose((FILE *)handle);
}

LNIFUNC void LNICALL lscript_io_StdFileHandle_fputc(LEnv venv, lclass vclazz, lulong handle, lchar c)
{
	fputc(c, (FILE *)handle);
}

LNIFUNC luint LNICALL lscript_io_StdFileHandle_fwrite(LEnv venv, lclass vclazz, lulong handle, lchararray data, luint off, luint length)
{
	luint temp = off + length;
	env_t *env = (env_t *)venv;
	if (!data)
	{
		env_raise_exception(env, exception_null_dereference, "data");
		return 0;
	}

	array_t *arr = (array_t *)data;
	char *cdata = (char *)&arr->data;

	FILE *nativeHandle = (FILE *)handle;

	luint result = 0;
	if (nativeHandle == stdout)
	{
		if (env->vm->stdio.writeStdoutFunc)
			result = (luint)env->vm->stdio.writeStdoutFunc(cdata, arr->length);
	}
	else if (nativeHandle == stderr)
	{
		if (env->vm->stdio.writeStderrFunc)
			result = (luint)env->vm->stdio.writeStderrFunc(cdata, arr->length);
	}
	else
		result = (luint)fwrite(cdata, sizeof(char), arr->length, (FILE *)handle);
	return result;
}

LNIFUNC luint LNICALL lscript_io_StdFileHandle_fread(LEnv venv, lclass vclazz, lulong handle, lchararray buf, luint off, luint length)
{
	env_t *env = (env_t *)venv;
	if (!buf)
	{
		env_raise_exception(env, exception_null_dereference, "buf");
		return 0;
	}
	
	array_t *arr = (array_t *)buf;
	luint readlen = length - off;
	if (off + readlen >= arr->length)
	{
		env_raise_exception(env, exception_bad_array_index, "read will cause buffer overun");
		return 0;
	}

	char *tempbuf = (char *)malloc(length);
	if (!tempbuf)
	{
		env_raise_exception(env, exception_out_of_memory, "on allocate temp buffer size %u", length);
		return 0;
	}

	FILE *nativeHandle = (FILE *)handle;

	luint result = 0;
	if (nativeHandle == stdin)
	{
		if (env->vm->stdio.writeStderrFunc)
			result = (luint)env->vm->stdio.writeStderrFunc(tempbuf, length);
	}
	else
		result = (luint)fread_s(tempbuf, length, sizeof(char), length, (FILE *)handle);

	memcpy((lchar *)(&arr->data) + off, tempbuf, length);
	free(tempbuf);

	return result;
}

LNIFUNC lchararray LNICALL lscript_io_StdFileHandle_freadline(LEnv venv, lclass vclazz, lulong handle)
{
	env_t *env = (env_t *)venv;
	char buf[256];

	FILE *nativeHandle = (FILE *)handle;
	if (nativeHandle == stdin)
	{
		if (!env->vm->stdio.readCharStdinFunc)
			return NULL;
		luint i;
		char c;
		for (i = 0; i < sizeof(buf) - 2 && (c = env->vm->stdio.readCharStdinFunc()) != '\n'; i++)
			buf[i] = c;
		buf[i] = '\n';
		buf[i + 1] = 0;
	}
	else
	{
		if (!fgets(buf, sizeof(buf), (FILE *)handle))
		{
			env_raise_exception(env, exception_illegal_state, "fgets returned NULL");
			return NULL;
		}
	}

	luint len = (luint)strlen(buf);
	array_t *arr = manager_alloc_array(env->vm->manager, lb_chararray, len);
	if (!arr)
	{
		env_raise_exception(env, exception_out_of_memory, "on alloc array length %u", len);
		return NULL;
	}

	memcpy(&arr->data, buf, len);

	return arr;
}
