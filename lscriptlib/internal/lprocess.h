#if !defined(LPROCESS_H)
#define LPROCESS_H

#include "../lscript.h"

void cleanup_processes();
LNIFUNC lulong LNICALL lscript_lang_Process_startProcess(LEnv venv, lclass vclazz, lobject processName, lobject commandLine, lobject workingDir);
LNIFUNC luint LNICALL lscript_lang_Process_getPID(LEnv venv, lclass vclazz, lulong nativeHandle);
LNIFUNC luint LNICALL lscript_lang_Process_wait(LEnv venv, lclass vclazz, lulong nativeHandle, luint length);
LNIFUNC lbool LNICALL lscript_lang_Process_isRunning(LEnv venv, lclass vclazz, lulong nativeHandle);
LNIFUNC lbool LNICALL lscript_lang_Process_stop(LEnv venv, lclass vclazz, lulong nativeHandle);
LNIFUNC lbool LNICALL lscript_lang_Process_forceStop(LEnv venv, lclass vclazz, lulong nativeHandle);
LNIFUNC void LNICALL lscript_lang_Process_freeProcessData(LEnv venv, lclass vclazz, lulong nativeHandle);

#endif