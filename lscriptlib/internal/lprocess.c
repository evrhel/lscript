#include "lprocess.h"

#include "vm.h"

#include <Windows.h>

typedef struct process_s process_t;
struct process_s
{
	STARTUPINFO *si;
	PROCESS_INFORMATION *pi;

	process_t *next;
	process_t *prev;
};

static process_t *g_processes = NULL;

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

LNIFUNC lulong LNICALL Process_startProcess(LEnv venv, lclass vclazz, lobject processName, lobject commandLine, lobject workingDir)
{
	env_t *env = (env_t *)venv;
	class_t *clazz = (class_t *)vclazz;

	array_t *processNameCharArrayField = (array_t *)object_get_object((object_t *)processName, "chars");
	array_t *commandLineCharArrayField = commandLine ? (array_t *)object_get_object((object_t *)commandLine, "chars") : NULL;
	array_t *workingDirCharArrayField = workingDir ? (array_t *)object_get_object((object_t *)workingDir, "chars") : NULL;

	char *cstrProcName, *cstrCommandLine, *cstrWorkingDir;

	cstrProcName = (char *)malloc(processNameCharArrayField->length + 1U);
	if (!cstrProcName)
	{
		env_raise_exception(env, exception_out_of_memory, "On allocate process name");
		return 0;
	}

	cstrCommandLine = commandLineCharArrayField ? (char *)malloc(commandLineCharArrayField->length + 1U) : NULL;
	cstrWorkingDir = workingDirCharArrayField ? (char *)malloc(workingDirCharArrayField->length + 1U) : NULL;

	memcpy(cstrProcName, &processNameCharArrayField->data, processNameCharArrayField->length);
	cstrProcName[processNameCharArrayField->length] = 0;

	if (cstrCommandLine)
	{
		memcpy(cstrCommandLine, &commandLineCharArrayField->data, commandLineCharArrayField->length);
		cstrCommandLine[commandLineCharArrayField->length] = 0;
	}

	if (cstrWorkingDir)
	{
		memcpy(cstrWorkingDir, &workingDirCharArrayField->data, workingDirCharArrayField->length);
		cstrWorkingDir[workingDirCharArrayField->length] = 0;
	}

	process_t *procStruct = (process_t *)malloc(sizeof(process_t));
	if (!procStruct)
	{
		free(cstrProcName);
		if (cstrCommandLine)
			free(cstrCommandLine);
		if (cstrWorkingDir)
			free(cstrWorkingDir);
		env_raise_exception(env, exception_out_of_memory, "On allocate process structure");
		return 0;
	}

	procStruct->si = (STARTUPINFO *)malloc(sizeof(STARTUPINFO));
	if (!procStruct->si)
	{
		free(procStruct);
		free(cstrProcName);
		if (cstrCommandLine)
			free(cstrCommandLine);
		if (cstrWorkingDir)
			free(cstrWorkingDir);
		env_raise_exception(env, exception_out_of_memory, "On allocate process startup info");
		return 0;
	}

	procStruct->pi = (PROCESS_INFORMATION *)malloc(sizeof(PROCESS_INFORMATION));
	if (!procStruct->pi)
	{
		free(procStruct->si);
		free(procStruct);
		free(cstrProcName);
		if (cstrCommandLine)
			free(cstrCommandLine);
		if (cstrWorkingDir)
			free(cstrWorkingDir);
		env_raise_exception(env, exception_out_of_memory, "On allocate process process info");
		return 0;
	}

	procStruct->next = NULL;
	procStruct->prev = NULL;

	ZeroMemory(procStruct->si, sizeof(STARTUPINFO));
	procStruct->si->cb = sizeof(STARTUPINFO);
	ZeroMemory(procStruct->pi, sizeof(PROCESS_INFORMATION));

	if (!CreateProcessA(
		cstrProcName,
		cstrCommandLine,
		NULL,
		NULL,
		FALSE,
		0,
		NULL,
		cstrWorkingDir,
		procStruct->si,
		procStruct->pi
	))
	{
		env_raise_exception(env, exception_illegal_state, "On CreateProcessA");
		return 0;
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

	return (lulong)procStruct;
}

LNIFUNC luint LNICALL Process_getPID(LEnv venv, lclass vclazz, lulong nativeHandle)
{
	process_t *proc = (process_t *)nativeHandle;
	return (int)proc->pi->dwProcessId;
}

LNIFUNC lint LNICALL Process_wait(LEnv venv, lclass vclazz, lulong nativeHandle, luint length)
{
	return WaitForSingleObject(((process_t *)nativeHandle)->pi->hProcess, length);
}

LNIFUNC lbool LNICALL Process_isRunning(LEnv venv, lclass vclazz, lulong nativeHandle)
{
	process_t *process = (process_t *)nativeHandle;

	DWORD exitCode;
	GetExitCodeProcess(process->pi->hProcess, &exitCode);
	return exitCode == STILL_ACTIVE;
}

LNIFUNC lbool LNICALL Process_stop(LEnv venv, lclass vclazz, lulong nativeHandle)
{
	process_t *process = (process_t *)nativeHandle;
	WaitForSingleObject(process->pi->hProcess, INFINITE);
	return TRUE;
}

LNIFUNC lbool LNICALL Process_forceStop(LEnv venv, lclass vclazz, lulong nativeHandle)
{
	process_t *process = (process_t *)nativeHandle;
	lbool result = TerminateProcess(process->pi->hProcess, -1);
	return result;
}

LNIFUNC void LNICALL Process_freeProcessData(LEnv venv, lclass vclazz, lulong nativeHandle)
{
	process_t *curr = g_processes;
	while (curr)
	{
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
		curr = curr->next;
	}
}
