#if !defined(COMPILER_H)
#define COMPILER_H

#include "collections.h"

compile_error_t *compile(input_file_t *files, const char *outputDirectory, unsigned int version, int debug, msg_func_t messenger, input_file_t **outputFiles);

#endif