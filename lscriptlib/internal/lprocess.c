#include "lprocess.h"

#include "vm.h"

#include <Windows.h>

LNIFUNC lulong LNICALL Process_startProcess(LEnv venv, lclass vclazz, lobject processName, lobject commandLine, lobject workingDir)
{
	env_t *env = (env_t *)venv;
	class_t *clazz = (class_t *)vclazz;

	

	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
}

LNIFUNC lint LNICALL Process_getPID(LEnv venv, lclass vclazz, lulong nativeHandle)
{
	
}

LNIFUNC lint LNICALL Process_wait(LEnv venv, lclass vclazz, lulong nativeHandle)
{
	
}

LNIFUNC lbool LNICALL Process_isRunning(LEnv venv, lclass vclazz, lulong nativeHandle)
{

}

LNIFUNC lint LNICALL Process_stop(LEnv venv, lclass vclazz, lulong nativeHandle)
{
	
}

LNIFUNC lint LNICALL Process_forceStop(LEnv venv, lclass vclazz, lulong nativeHandle)
{
	
}
