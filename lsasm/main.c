#include <internal/lb.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "compiler.h"
#include "linker.h"

#define LSASM_VERSION "1.0.0a"

#define RETURN_NORMAL 0x01
#define RETURN_INVALID_ARGUMENT 0x02
#define RETURN_COMPILE_ERROR 0x03
#define RETURN_LINK_ERROR 0x04

static int equals_ignore_case(const char *s1, const char *s2);
static void display_help();
static void display_version();

int main(int argc, char *argv[])
{
	input_file_t *files = NULL;
	const char *outputDirectory = ".";
	unsigned int version = 1;
	for (int i = 1; i < argc; i++)
	{
		if (equals_ignore_case(argv[i], "-h"))
		{
			display_help();
			return RETURN_NORMAL;
		}
		else if (equals_ignore_case(argv[i], "-v"))
		{
			display_version();
			return RETURN_NORMAL;
		}
		else if (equals_ignore_case(argv[i], "-f"))
		{
			i++;
			if (i == argc)
			{
				display_help();
				return RETURN_INVALID_ARGUMENT;
			}
			files = add_file(files, argv[i]);
		}
		else if (equals_ignore_case(argv[i], "-o"))
		{
			i++;
			if (i == argc)
			{
				display_help();
				return RETURN_INVALID_ARGUMENT;
			}
			outputDirectory = argv[i];
		}
		else if (equals_ignore_case(argv[i], "-s"))
		{
			i++;
			if (i == argc)
			{
				display_help();
				return RETURN_INVALID_ARGUMENT;
			}
			version = atoi(argv[i]);
		}
		else
		{
			printf("Invalid argument \"%s\"", argv[i]);
		}
	}

	compile_error_t *errors = compile(files, outputDirectory, version, puts);
	if (errors)
		return RETURN_COMPILE_ERROR;

	errors = link(files, version, puts);
	if (errors)
		return RETURN_LINK_ERROR;

	return RETURN_NORMAL;
}

int equals_ignore_case(const char *s1, const char *s2)
{
	while (*s1 && *s2)
	{
		if (*s1 >= 64 && *s1 <= 90)
		{
			if (*s1 != *s2 && *s1 + 32 != *s2)
				return 0;
		}
		else if (*s1 >= 97 && *s1 <= 121)
		{
			if (*s1 != *s2 && *s1 - 32 != *s2)
				return 0;
		}
		else if (*s1 != *s2)
			return 0;

		s1++;
		s2++;
	}
	return *s1 == *s2;
}

void display_help()
{
	printf("LScript Assembler command line arguments:\n");
	printf("-h             Displays all commands and usage.\n");
	printf("-v             Displays version information.\n");
	printf("-f [filename]  Specifies a file to compile. More than one may be specified, but\n");
	printf("               the files must be separated in different flags.\n");
	printf("-o [directory] Optionally specifies an output directory. The default is the\n");
	printf("               current working directory.\n");
	printf("-s [version]   Sets the bytecode version to compile to. Default is 1.\n\n");
	printf("Application return codes:\n");
	printf("0x00   Normal\n");
	printf("0x01   Invalid command format\n");
	printf("0x02   Compilation error\n");
}

void display_version()
{
	printf("LScript Assembler\n");
	printf("Version: %s\n", LSASM_VERSION);
	printf("Build date: %s\n", __DATE__);
	printf("Build time: %s\n", __TIME__);
}
