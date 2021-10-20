#include "file_util.h"

#include <Windows.h>

int create_intermediate_directories(const char *path)
{
    char createPath[MAX_PATH];
    strcpy_s(createPath, sizeof(createPath), path);

    char *curr = createPath;
    while (curr = strchr(curr, '\\'))
    {
        *curr = 0;
        CreateDirectoryA(createPath, NULL);
        *curr = '\\';
        curr++;
    }
    CreateDirectoryA(createPath, NULL);

    return 1;
}
