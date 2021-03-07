#if !defined(LPROCESS_H)
#define LPROCESS_H

#include "../lscript.h"

LNIFUNC lulong LNICALL Process_startProcess(LEnv venv, lclass vclazz, lobject processName, lobject commandLine, lobject workingDir);
LNIFUNC lint LNICALL Process_getPID(LEnv venv, lclass vclazz, lulong nativeHandle);
LNIFUNC lint LNICALL Process_wait(LEnv venv, lclass vclazz, lulong nativeHandle);
LNIFUNC lbool LNICALL Process_isRunning(LEnv venv, lclass vclazz, lulong nativeHandle);
LNIFUNC lint LNICALL Process_stop(LEnv venv, lclass vclazz, lulong nativeHandle);
LNIFUNC lint LNICALL Process_forceStop(LEnv venv, lclass vclazz, lulong nativeHandle);

#endif