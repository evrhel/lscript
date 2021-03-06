#include <lscript.h>

#include <stdio.h>
#include <Windows.h>

#include "internal/lprocess.h"


int main(int argc, char *argv[])
{
	LVM vm;

	HANDLE hThreadHandle;
	DWORD dThreadID;

	HMODULE lsAPILib;

	lsAPILib = GetModuleHandleA("lscriptapi.dll");
	if (!lsAPILib)
	{
		printf("Failed to locate lscriptapi.dll");
		return 0xc0;
	}

	ls_init();

	vm = ls_create_and_start_vm(argc - 1, argv + 1, &hThreadHandle, &dThreadID, lsAPILib, NULL);
	if (!vm)
	{
		printf("Failed to start virtual machine");
		return 0xc1;
	}

	ls_destroy_vm(INFINITE);

	ls_done();

	return 0;
}
