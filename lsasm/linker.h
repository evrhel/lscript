#if !defined(LINKER_H)
#define LINKER_H

#include "collections.h"
#include <internal/types.h>

compile_error_t *link(input_file_t *files, unsigned int linkVersion, msg_func_t messenger);

#endif