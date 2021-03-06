#if !defined(COMPILER_H)
#define COMPILER_H

#include "collections.h"

enum
{
	align_none			=	0x00000000,
	align_functions		=	0x00000001,
	align_globals		=	0x00000002,

	align_all			=	-1
};

typedef union alignment_u
{
	int value;
	struct
	{
		unsigned char functionAlignment;
		unsigned char globalAlignment;
	};
} alignment_t;

compile_error_t *compile(input_file_t *files, const char *outputDirectory, unsigned int version,
	int debug, alignment_t alignment, msg_func_t messenger, input_file_t **outputFiles);

#endif