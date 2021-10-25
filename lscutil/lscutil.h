#if !defined(LSCUTIL_H)
#define LSCUTIL_H

#include <stdlib.h>

#define LSCUEXPORT __declspec(dllexport)

#define LSCU_MAX_CLASSPATHS 16
#define LSCU_MAX_IMPORTS 32

typedef void *LSCUCONTEXT;

LSCUEXPORT LSCUCONTEXT lscu_init();
LSCUEXPORT void lscu_destroy(LSCUCONTEXT context);

LSCUEXPORT int lscu_add_classpath(LSCUCONTEXT context, const char *__restrict path);
LSCUEXPORT int lscu_add_unimportant_classpath(LSCUCONTEXT context, const char *__restrict path);
LSCUEXPORT void lscu_remove_all_classpaths(LSCUCONTEXT context);

LSCUEXPORT void lscu_set_package(LSCUCONTEXT context, const char *__restrict package);
LSCUEXPORT int lscu_add_import(LSCUCONTEXT context, const char *__restrict package);
LSCUEXPORT void lscu_remove_all_imports(LSCUCONTEXT context);

LSCUEXPORT int lscu_resolve_class(LSCUCONTEXT context, const char *__restrict classname, char *__restrict fullname, size_t bufsize);

#endif