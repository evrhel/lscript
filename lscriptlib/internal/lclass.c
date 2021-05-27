#include "lclass.h"

#include "vm.h"

LNIFUNC lobject LNICALL Class_classOf(LEnv venv, lclass cclazz, lobject object)
{
	if (!object)
	{
		env_raise_exception((env_t *)venv, exception_null_dereference, NULL);
		return NULL;
	}
	env_t *env = (env_t *)venv;
	object_t *obj = (object_t *)object;
	return vm_get_class_object(env->vm, obj->clazz->name);
}

