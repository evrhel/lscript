#include "lobject.h"

#include "vm.h"

LNIFUNC lobject LNICALL Object_getClass(LEnv venv, lclass cclazz, lobject object)
{
	if (!object)
	{
		env_raise_exception((env_t *)venv, exception_null_dereference, NULL);
		return;
	}
	return ((object_t *)object)->clazz;
}
