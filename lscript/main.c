#include <lscript.h>

#include <Windows.h>

#include "internal/lprocess.h"


int main(int argc, char *argv[])
{
	LVM vm;

	HANDLE hThreadHandle;
	DWORD dThreadID;

	int initResult;
	HMODULE lsAPILib;

	lsAPILib = GetModuleHandleA("lscriptapi.dll");
	if (!lsAPILib)
		return 0xc0;

	vm = ls_create_and_start_vm(argc - 1, argv + 1, &hThreadHandle, &dThreadID, lsAPILib);
	if (!vm)
		return 0xc1;

	ls_destroy_vm(INFINITE);

	return 0;
}
