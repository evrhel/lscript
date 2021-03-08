#if !defined(LPROCESS_H)
#define LPROCESS_H

#include "../lscript.h"

void cleanup_processes();
LNIFUNC void LNICALL Process_test(LEnv venv, lclass vclazz, lint var);
LNIFUNC lulong LNICALL Process_startProcess(LEnv venv, lclass vclazz, lobject processName, lobject commandLine, lobject workingDir);
LNIFUNC luint LNICALL Process_getPID(LEnv venv, lclass vclazz, lulong nativeHandle);
LNIFUNC lint LNICALL Process_wait(LEnv venv, lclass vclazz, lulong nativeHandle, luint length);
LNIFUNC lbool LNICALL Process_isRunning(LEnv venv, lclass vclazz, lulong nativeHandle);
LNIFUNC lbool LNICALL Process_stop(LEnv venv, lclass vclazz, lulong nativeHandle);
LNIFUNC lbool LNICALL Process_forceStop(LEnv venv, lclass vclazz, lulong nativeHandle);
LNIFUNC void LNICALL Process_freeProcessData(LEnv venv, lclass vclazz, lulong nativeHandle);

#endif