#if !defined(COMPILER_H)
#define COMPILER_H

#include "collections.h"

compile_error_t *compile(input_file_t *files, const char *outputDirectory, unsigned int version, msg_func_t messenger);

#endif