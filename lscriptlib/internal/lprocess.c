#include "lprocess.h"

#include "vm.h"

#include <Windows.h>

#include <stdio.h>

#define CMD_LINE_PADDING 512ULL

typedef struct process_start_params_s process_start_params_t;
struct process_start_params_s
{
	env_t *env;
	const char *processName;
	char *commandLine;
	const char *workingDir;
};

typedef struct process_s process_t;
struct process_s
{
	LPSTARTUPINFOA si;
	LPPROCESS_INFORMATION pi;

	CHAR procName[MAX_PATH];

	process_t *next;
	process_t *prev;
};

static process_t *g_processReturn = NULL;
static process_t *g_processes = NULL;
static char g_commandLine[32767]; // A buffer for the command line when a process starts

static void start_process_from_thread(process_start_params_t *startParams);
static process_t *start_process(env_t *env, const char *processName, char *commandLine, const char *workingDir);

void cleanup_processes()
{
	process_t *curr = g_processes;
	process_t *next;
	while (curr)
	{
		next = curr->next;
		CloseHandle(curr->pi->hProcess);
		CloseHandle(curr->pi->hThread);
		free(curr->pi);
		free(curr->si);
		free(curr);
		curr = next;
	}
}

LNIFUNC lulong LNICALL lscript_lang_Process_startProcess(LEnv venv, lclass vclazz, lobject processName, lobject commandLine, lobject workingDir)
{
	env_t *env = (env_t *)venv;
	class_t *clazz = (class_t *)vclazz;

	array_t *processNameCharArrayField = processName ? (array_t *)object_get_object((object_t *)processName, "chars") : NULL;
	array_t *commandLineCharArrayField = commandLine ? (array_t *)object_get_object((object_t *)commandLine, "chars") : NULL;
	array_t *workingDirCharArrayField = workingDir ? (array_t *)object_get_object((object_t *)workingDir, "chars") : NULL;

	process_start_params_t startParams;

	char procName[MAX_PATH];
	char *cstrWorkingDir;

	cstrWorkingDir = workingDirCharArrayField ? (char *)malloc((size_t)workingDirCharArrayField->length + 1ULL) : NULL;

	if (workingDirCharArrayField && !cstrWorkingDir)
	{
		free(cstrWorkingDir);
		env_raise_exception(env, exception_out_of_memory, "On allocate in startProcess");
		return 0;
	}

	if (processNameCharArrayField)
	{
		ZeroMemory(procName, sizeof(procName));
		memcpy(procName, &processNameCharArrayField->data, processNameCharArrayField->length);
	}

	if (commandLineCharArrayField)
	{
		ZeroMemory(g_commandLine, sizeof(g_commandLine));
		memcpy(g_commandLine, &commandLineCharArrayField->data, commandLineCharArrayField->length);
	}

	startParams.env = env;
	startParams.processName = processNameCharArrayField ? procName : NULL;
	startParams.commandLine = g_commandLine;
	startParams.workingDir = cstrWorkingDir;

	if (cstrWorkingDir)
	{
		memcpy(cstrWorkingDir, &workingDirCharArrayField->data, workingDirCharArrayField->length);
		cstrWorkingDir[workingDirCharArrayField->length] = 0;
	}

	// For some reason the process can only be created on a separate thread...
	// I have no idea why :P

	HANDLE handle = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)start_process_from_thread, &startParams, 0, 0);
	if (!handle)
	{
		free(cstrWorkingDir);
		env_raise_exception(env, exception_illegal_state, "Failed to start thread to start process");
		return 0;
	}

	// Wait for the process to be created
	WaitForSingleObject(handle, INFINITE);
	CloseHandle(handle);

	process_t *processReturn = g_processReturn;
	g_processReturn = NULL;

	free(cstrWorkingDir);
	
	return (lulong)processReturn;
}

LNIFUNC luint LNICALL lscript_lang_Process_getPID(LEnv venv, lclass vclazz, lulong nativeHandle)
{
	process_t *proc = (process_t *)nativeHandle;
	return (int)proc->pi->dwProcessId;
}

LNIFUNC luint LNICALL lscript_lang_Process_wait(LEnv venv, lclass vclazz, lulong nativeHandle, luint length)
{
	return WaitForSingleObject(((process_t *)nativeHandle)->pi->hProcess, length);
}

LNIFUNC lbool LNICALL lscript_lang_Process_isRunning(LEnv venv, lclass vclazz, lulong nativeHandle)
{
	process_t *process = (process_t *)nativeHandle;

	DWORD exitCode;
	GetExitCodeProcess(process->pi->hProcess, &exitCode);
	return exitCode == STILL_ACTIVE;
}

LNIFUNC lbool LNICALL lscript_lang_Process_stop(LEnv venv, lclass vclazz, lulong nativeHandle)
{
	process_t *process = (process_t *)nativeHandle;
	WaitForSingleObject(process->pi->hProcess, INFINITE);
	return TRUE;
}

LNIFUNC lbool LNICALL lscript_lang_Process_forceStop(LEnv venv, lclass vclazz, lulong nativeHandle)
{
	process_t *process = (process_t *)nativeHandle;
	lbool result = TerminateProcess(process->pi->hProcess, -1);
	return result;
}

LNIFUNC void LNICALL lscript_lang_Process_freeProcessData(LEnv venv, lclass vclazz, lulong nativeHandle)
{
	process_t *curr = g_processes;
	process_t *next;
	while (curr)
	{
		next = curr->next;
		if (curr = (process_t *)nativeHandle)
		{
			if (curr->next)
				curr->next->prev = curr->prev;

			if (curr->prev)
				curr->prev->next = curr->next;

			if (!curr->prev)
				g_processes = curr->next;

			CloseHandle(curr->pi->hProcess);
			CloseHandle(curr->pi->hThread);
			free(curr->pi);
			free(curr->si);
			free(curr);
		}
		curr = next;
	}
}

void start_process_from_thread(process_start_params_t *startParams)
{
	g_processReturn = start_process(startParams->env, startParams->processName, startParams->commandLine, startParams->workingDir);
}

process_t *start_process(env_t *env, const char *processName, char *commandLine, const char *workingDir)
{
	process_t *procStruct = (process_t *)malloc(sizeof(process_t));
	if (!procStruct)
	{
		env_raise_exception(env, exception_out_of_memory, "On allocate process structure");
		return NULL;
	}

	procStruct->si = (LPSTARTUPINFOA)malloc(sizeof(STARTUPINFOA));
	if (!procStruct->si)
	{
		free(procStruct);
		env_raise_exception(env, exception_out_of_memory, "On allocate process startup info");
		return NULL;
	}

	procStruct->pi = (LPPROCESS_INFORMATION)malloc(sizeof(PROCESS_INFORMATION));
	if (!procStruct->pi)
	{
		free(procStruct->si);
		free(procStruct);
		env_raise_exception(env, exception_out_of_memory, "On allocate process process info");
		return NULL;
	}

	ZeroMemory(procStruct->procName, sizeof(procStruct->procName));
	if (processName)
		strcpy_s(procStruct->procName, sizeof(procStruct->procName), processName);
	procStruct->next = NULL;
	procStruct->prev = NULL;

	ZeroMemory(procStruct->si, sizeof(*(procStruct->si)));
	procStruct->si->cb = sizeof(*(procStruct->si));
	ZeroMemory(procStruct->pi, sizeof(*(procStruct->pi)));

	if (!CreateProcessA(
		processName,
		commandLine,
		NULL,
		NULL,
		FALSE,
		0,
		NULL,
		workingDir,
		procStruct->si,
		procStruct->pi
	))
	{
		env_raise_exception(env, exception_illegal_state, "On CreateProcessA");
		return NULL;
	}

	if (g_processes)
	{
		g_processes->prev = procStruct;
		procStruct->next = g_processes;
		g_processes = procStruct;
	}
	else
	{
		g_processes = procStruct;
	}
	return procStruct;
}
