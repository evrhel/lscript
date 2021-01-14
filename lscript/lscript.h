#if !defined(LSCRIPT_H)
#define LSCRIPT_H

#include <varargs.h>

#define LEXPORT __declspec(dllexport)
#define LFUNC __cdecl

typedef void lvoid;
typedef char lchar;
typedef unsigned char luchar;
typedef short lshort;
typedef unsigned short lushort;
typedef int lint;
typedef unsigned int luint;
typedef long long llong;
typedef unsigned long long lulong;
typedef char lbool;
typedef float lfloat;
typedef double ldouble;
typedef void *lobject;

typedef void *lchararray;
typedef void *luchararray;
typedef void *lshortarray;
typedef void *lushortarray;
typedef void *lintarray;
typedef void *luintarray;
typedef void *llongarray;
typedef void *lulongarray;
typedef void *lboolarray;
typedef void *lfloatarray;
typedef void *ldoublearray;
typedef void *lobjectarray;

typedef void *lclass;

typedef void *LVM;
typedef void *LEnv;

typedef void *lfield;
typedef void *lfunction;

LEXPORT LVM LFUNC ls_create_vm(int argc, const char *const argv[]);
LEXPORT void LFUNC ls_destroy_vm(LVM vm);

LEXPORT void LFUNC ls_add_to_classpath(const char *path);

LEXPORT lbool LFUNC ls_load_class_file(const char *filepath);
LEXPORT lbool LFUNC ls_load_class_data(const char *data, luint datalen);
LEXPORT lbool LFUNC ls_load_class_name(const lchar *classname);

LEXPORT lclass LFUNC ls_class_for_name(const lchar *classname);
LEXPORT lfield LFUNC ls_get_field(lclass clazz, const lchar *name);
LEXPORT lfunction LFUNC ls_get_function(lclass clazz, const lchar *name, const lchar *sig);

LEXPORT lvoid LFUNC ls_call_void_function(lfunction function, lobject object, ...);
LEXPORT lvoid LFUNC ls_call_void_functionv(lfunction function, lobject object, va_list list);

LEXPORT lvoid LFUNC ls_call_static_void_function(lfunction function, lclass clazz, ...);
LEXPORT lvoid LFUNC ls_call_static_void_functionv(lfunction function, lclass clazz, va_list list);

#endif