#if !defined(LSYSTEM_H)
#define LSYSTEM_H

#include "../lscript.h"

LNIFUNC void LNICALL lscript_lang_System_exit(LEnv venv, lclass vclazz, lint code);

LNIFUNC void LNICALL lscript_lang_System_loadLibrary(LEnv venv, lclass vclazz, lobject libname);

LNIFUNC lobject LNICALL lscript_lang_System_getProperty(LEnv venv, lclass vclazz, lobject propName);

LNIFUNC lobject LNICALL lscript_lang_System_setProperty(LEnv venv, lclass vclazz, lobject propName, lobject value);

LNIFUNC lobject LNICALL lscript_lang_System_arraycopy(LEnv venv, lclass vclazz, lobject dst, luint dstOff, lobject src, luint srcOff, luint len);

#endif