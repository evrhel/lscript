#if !defined(LSCRIPT_H)
#define LSCRIPT_H

#include <stdlib.h>
#include <stdarg.h>

#define LS_VERSION "1.0.0a"

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

#define DEFAULT_WRITE_STDOUT (ls_write_func)(-1)
#define DEFAULT_WRITE_STDERR (ls_write_func)(-2)
#define DEFAULT_READ_STDIN (ls_read_func)(-3)
#define DEFAULT_READ_CHAR_STDIN (ls_read_char_func)(-4)

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
 
	typedef size_t(*ls_write_func)(const char *buf, size_t count);
	typedef size_t(*ls_read_func)(char *buf, size_t size, size_t count);
	typedef char(*ls_read_char_func)();

	typedef struct ls_stdio_s
	{
		ls_write_func writeStdoutFunc;
		ls_write_func writeStderrFunc;
		ls_read_func readStdinFunc;
		ls_read_char_func readCharStdinFunc;
	} ls_stdio_t;

	LEXPORT LVM LCALL ls_create_vm(int argc, const char *const argv[], void *lsAPILib, const ls_stdio_t *stdio);
	LEXPORT lint LCALL ls_start_vm(int argc, const char *const argv[], void **threadHandle, unsigned long *threadID);
	LEXPORT LVM LCALL ls_create_and_start_vm(int argc, const char *const argv[], void **threadHandle, unsigned long *threadID, void *lsAPILib, const ls_stdio_t *stdio);
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

	LEXPORT lchar LCALL ls_call_char_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline lchar LCALL ls_call_char_function(LEnv env, lfunction function, lobject object, ...)
	{
		lchar result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_char_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT luchar LCALL ls_call_uchar_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline luchar LCALL ls_call_uchar_function(LEnv env, lfunction function, lobject object, ...)
	{
		luchar result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_uchar_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT lshort LCALL ls_call_short_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline lshort LCALL ls_call_short_function(LEnv env, lfunction function, lobject object, ...)
	{
		lshort result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_short_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT lushort LCALL ls_call_ushort_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline lushort LCALL ls_call_ushort_function(LEnv env, lfunction function, lobject object, ...)
	{
		lushort result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_ushort_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT lint LCALL ls_call_int_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline lint LCALL ls_call_int_function(LEnv env, lfunction function, lobject object, ...)
	{
		lint result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_int_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT luint LCALL ls_call_uint_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline luint LCALL ls_call_uint_function(LEnv env, lfunction function, lobject object, ...)
	{
		luint result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_uint_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT llong LCALL ls_call_long_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline llong LCALL ls_call_long_function(LEnv env, lfunction function, lobject object, ...)
	{
		llong result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_long_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT lulong LCALL ls_call_ulong_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline lulong LCALL ls_call_ulong_function(LEnv env, lfunction function, lobject object, ...)
	{
		lulong result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_ulong_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT lbool LCALL ls_call_bool_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline lbool LCALL ls_call_bool_function(LEnv env, lfunction function, lobject object, ...)
	{
		lbool result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_bool_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT lfloat LCALL ls_call_float_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline lfloat LCALL ls_call_float_function(LEnv env, lfunction function, lobject object, ...)
	{
		lfloat result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_float_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT ldouble LCALL ls_call_double_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline ldouble LCALL ls_call_double_function(LEnv env, lfunction function, lobject object, ...)
	{
		ldouble result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_double_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT lobject LCALL ls_call_object_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline lobject LCALL ls_call_object_function(LEnv env, lfunction function, lobject object, ...)
	{
		lobject result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_object_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT lvoid LCALL ls_call_static_void_functionv(LEnv env, lfunction function, va_list list);
	inline lvoid LCALL ls_call_static_void_function(LEnv env, lfunction function, ...)
	{
		va_list ls;
		va_start(ls, function);
		ls_call_static_void_functionv(env, function, ls);
		va_end(ls);
	}

	LEXPORT lchar LCALL ls_call_static_char_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline lchar LCALL ls_call_static_char_function(LEnv env, lfunction function, lobject object, ...)
	{
		lchar result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_static_char_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT luchar LCALL ls_call_static_uchar_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline luchar LCALL ls_call_static_uchar_function(LEnv env, lfunction function, lobject object, ...)
	{
		luchar result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_static_uchar_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT lshort LCALL ls_call_static_short_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline lshort LCALL ls_call_static_short_function(LEnv env, lfunction function, lobject object, ...)
	{
		lshort result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_static_short_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT lushort LCALL ls_call_static_ushort_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline lushort LCALL ls_call_static_ushort_function(LEnv env, lfunction function, lobject object, ...)
	{
		lushort result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_static_ushort_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT lint LCALL ls_call_static_int_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline lint LCALL ls_call_static_int_function(LEnv env, lfunction function, lobject object, ...)
	{
		lint result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_static_int_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT luint LCALL ls_call_static_uint_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline luint LCALL ls_call_static_uint_function(LEnv env, lfunction function, lobject object, ...)
	{
		luint result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_static_uint_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT llong LCALL ls_call_static_long_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline llong LCALL ls_call_static_long_function(LEnv env, lfunction function, lobject object, ...)
	{
		llong result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_static_long_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT lulong LCALL ls_call_static_ulong_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline lulong LCALL ls_call_static_ulong_function(LEnv env, lfunction function, lobject object, ...)
	{
		lulong result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_static_ulong_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT lbool LCALL ls_call_static_bool_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline lbool LCALL ls_call_static_bool_function(LEnv env, lfunction function, lobject object, ...)
	{
		lbool result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_static_bool_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT lfloat LCALL ls_call_static_float_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline lfloat LCALL ls_call_static_float_function(LEnv env, lfunction function, lobject object, ...)
	{
		lfloat result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_static_float_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT ldouble LCALL ls_call_static_double_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline ldouble LCALL ls_call_static_double_function(LEnv env, lfunction function, lobject object, ...)
	{
		ldouble result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_static_double_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT lobject LCALL ls_call_static_object_functionv(LEnv env, lfunction function, lobject object, va_list list);
	inline lobject LCALL ls_call_static_object_function(LEnv env, lfunction function, lobject object, ...)
	{
		lobject result;
		va_list ls;
		va_start(ls, object);
		result = ls_call_static_object_functionv(env, function, object, ls);
		va_end(ls);
		return result;
	}

	LEXPORT luint LCALL ls_get_array_length(lobject array);

#if defined(__cplusplus)
}
#endif

#endif