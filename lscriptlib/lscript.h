#if !defined(LSCRIPT_H)
#define LSCRIPT_H

#include <stdarg.h>

#define LEXPORT __declspec(dllexport)
#define LCALL __cdecl

#define LNIFUNC __declspec(dllexport)
#define LNICALL __stdcall

#define LIMPORT __declspec(dllimport)

#define KB_TO_B(KB) ((KB)*1024ULL)
#define MB_TO_B(MB) (KB_TO_B((MB)*1024ULL))
#define GB_TO_B(GB) (MB_TO_B((GB)*1024ULL))

#define DEFAULT_HEAP_SIZE GB_TO_B(2)
#define DEFAULT_STACK_SIZE KB_TO_B(2)

#if defined(__cplusplus)
extern "C"
{
#endif

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

	LEXPORT lint LCALL ls_init();

	LEXPORT LVM LCALL ls_create_vm(int argc, const char *const argv[], void *lsAPILib);
	LEXPORT lint LCALL ls_start_vm(int argc, const char *const argv[], void **threadHandle, unsigned long *threadID);
	LEXPORT LVM LCALL ls_create_and_start_vm(int argc, const char *const argv[], void **threadHandle, unsigned long *threadID, void *lsAPILib);
	LEXPORT lvoid LCALL ls_destroy_vm(unsigned long threadWaitTime);
	LEXPORT LVM LCALL ls_get_current_vm();

	LEXPORT lvoid LCALL ls_add_to_classpath(const char *path);

	LEXPORT lclass LCALL ls_load_class_file(const char *filepath);
	LEXPORT lclass LCALL ls_load_class_data(unsigned char *data, luint datalen);
	LEXPORT lclass LCALL ls_load_class_name(const lchar *classname);

	LEXPORT lclass LCALL ls_class_for_name(const lchar *classname);
	LEXPORT lfield LCALL ls_get_field(lclass clazz, const lchar *name);
	LEXPORT lfunction LCALL ls_get_function(lclass clazz, const lchar *qualifiedName);

	LEXPORT lvoid LCALL ls_call_void_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline lvoid LCALL ls_call_void_function(LEnv env, lfunction function, lobject object, ...)
	{
		va_list ls;
		va_start(ls, object);
		ls_call_void_functionv(env, function, object, ls);
		va_end(ls);
	}

	LEXPORT lvoid LCALL ls_call_static_void_functionv(LEnv env, lfunction function, va_list list);
	inline lvoid LCALL ls_call_static_void_function(LEnv env, lfunction function, ...)
	{
		va_list ls;
		va_start(ls, function);
		ls_call_static_void_functionv(env, function, ls);
		va_end(ls);
	}

	LEXPORT luint LCALL ls_get_array_length(lobject array);

#if defined(__cplusplus)
}
#endif

#endif