#include <lscript.h>

#include <Windows.h>

int main(int argc, char *argv[])
{
	LVM vm;

	HANDLE hThreadHandle;
	DWORD dThreadID;

	vm = ls_create_and_start_vm(argc - 1, argv + 1, &hThreadHandle, &dThreadID);

	ls_destroy_vm(INFINITE);

	return 0;
}