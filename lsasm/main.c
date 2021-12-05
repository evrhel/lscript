#include <internal/lb.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <Windows.h>
#include <strutil/strutil.h>

#include "compiler.h"
#include "linker.h"
#include "buffer.h"

#define LSASM_VERSION "1.0.0"

#define RETURN_NORMAL 0x01
#define RETURN_INVALID_ARGUMENT 0x02
#define RETURN_COMPILE_ERROR 0x03
#define RETURN_LINK_ERROR 0x04

static void display_help();
static void display_version();
static int are_errors(const compile_error_t *list);
static input_file_t *add_source_files_in_directory(const char *directory, input_file_t *files, int recursive, const char *unitPrefix);

int main(int argc, char *argv[])
{
	compiler_options_t options;
	memset(&options, 0, sizeof(options));

	input_file_t *files = NULL;
	const char *inputDirectory = NULL;
	const char *outputDirectory = ".";
	unsigned int version = 1;
	int runCompiler = 1, runLinker = 1;
	int compileDebug = 0;
	int inputDirRec = 0;
	alignment_t alignment;
	alignment.functionAlignment = 32;
	alignment.globalAlignment = 8;

	int readingInputs = 0;

	BEGIN_DEBUG();


	for (int i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-' && readingInputs)
		{
			display_help();
			return RETURN_INVALID_ARGUMENT;
		}
		else if (argv[i][0] != '-')
			readingInputs = 1;

		if (str_equals_ignore_case(argv[i], "-help") || str_equals_ignore_case(argv[i], "-h") || str_equals_ignore_case(argv[i], "-?"))
		{
			display_help();
			return RETURN_NORMAL;
		}
		else if (str_equals_ignore_case(argv[i], "-version") || str_equals_ignore_case(argv[i], "-v"))
		{
			display_version();
			return RETURN_NORMAL;
		}
		else if (str_equals_ignore_case(argv[i], "-o"))
		{
			i++;
			if (i == argc)
			{
				display_help();
				return RETURN_INVALID_ARGUMENT;
			}
			outputDirectory = argv[i];
		}
		else if (str_equals_ignore_case(argv[i], "-i"))
		{
			i++;
			if (i == argc)
			{
				display_help();
				return RETURN_INVALID_ARGUMENT;
			}
			inputDirectory = argv[i];

			for (int j = 0; j < LSCU_MAX_CLASSPATHS; j++)
			{
				if (!options.classpaths[j][0])
				{
					strcpy_s(options.classpaths[j], sizeof(options.classpaths[j]), inputDirectory);
					break;
				}
			}
		}
		else if (str_equals_ignore_case(argv[i], "-r"))
		{
			inputDirRec = 1;
		}
		else if (str_equals_ignore_case(argv[i], "-cp"))
		{
			i++;
			if (i == argc)
			{
				display_help();
				return RETURN_INVALID_ARGUMENT;
			}

			for (int j = 0; j < LSCU_MAX_CLASSPATHS; j++)
			{
				if (!options.classpaths[j][0])
				{
					strcpy_s(options.classpaths[j], sizeof(options.classpaths[j]), argv[i]);
					break;
				}
			}
		}
		else if (str_equals_ignore_case(argv[i], "-s"))
		{
			i++;
			if (i == argc)
			{
				display_help();
				return RETURN_INVALID_ARGUMENT;
			}
			version = atoi(argv[i]);
		}
		else if (str_equals_ignore_case(argv[i], "-fa"))
		{
			i++;
			if (i == argc)
			{
				display_help();
				return RETURN_INVALID_ARGUMENT;
			}
			alignment.functionAlignment = (unsigned char)atoi(argv[i]);
			if (alignment.functionAlignment == 0)
				alignment.functionAlignment = 1;
		}
		else if (str_equals_ignore_case(argv[i], "-ga"))
		{
			i++;
			if (i == argc)
			{
				display_help();
				return RETURN_INVALID_ARGUMENT;
			}
			alignment.globalAlignment = (unsigned char)atoi(argv[i]);
			if (alignment.globalAlignment == 0)
				alignment.globalAlignment = 1;
		}
		else if (str_equals_ignore_case(argv[i], "-nc"))
		{
			runCompiler = 0;
		}
		else if (str_equals_ignore_case(argv[i], "-nl"))
		{
			runLinker = 0;
		}
		else if (str_equals_ignore_case(argv[i], "-d"))
		{
			compileDebug = 1;
		}
		else if (readingInputs)
		{
			files = add_file(files, argv[i], argv[i]);
		}
		else
		{
			printf("Invalid switch: %s\n", argv[i]);
			display_help();
			return RETURN_INVALID_ARGUMENT;
		}
	}

	if (inputDirectory)
		files = add_source_files_in_directory(inputDirectory, files, inputDirRec, NULL);

	if (!files)
	{
		printf("No input files.");
		return RETURN_INVALID_ARGUMENT;
	}

	compile_error_t *errors = NULL;
	input_file_t *linkFiles = NULL;

	if (runCompiler)
	{
		printf("[BEGIN COMPILE]\n");

		options.inFiles = files;
		options.outDirectory = outputDirectory;
		options.version = version;
		options.debug = compileDebug;
		options.alignment = alignment;
		options.messenger = (msg_func_t)puts;
		options.outputFiles = &linkFiles;

		errors = compile(&options);
		free_file_list(files);

		if (are_errors(errors))
		{
			free_compile_error_list(errors);
			END_DEBUG();
			return RETURN_COMPILE_ERROR;
		}
		free_compile_error_list(errors);
		putc('\n', stdout);
	}

	if (runLinker)
	{
		printf("[BEGIN LINK]\n");
		errors = link(linkFiles, version, (msg_func_t)puts);

		free_file_list(linkFiles);

		if (are_errors(errors))
		{
			free_compile_error_list(errors);
			END_DEBUG();
			return RETURN_LINK_ERROR;
		}
		free_compile_error_list(errors);
	}
	else
		free_file_list(linkFiles);

	END_DEBUG();
	return RETURN_NORMAL;
}

void display_help()
{
	printf("LScript Assembler\n");
	printf("Usage: lsasm [options...] [files...]\n\n");
	printf("Where [options...] include:\n");
	printf("-help -h -?    Prints this help message.\n");
	printf("-version -v    Displays version information.\n");
	printf("-o [directory] Optionally specifies an output directory. The default is the\n");
	printf("               current working directory.\n");
	printf("-i [directory] Sets the directory containing source files. All files with the\n");
	printf("               .lasm extension will be added as an input compilation.\n");
	printf("-r             Indicates that the directory specified in -i should search\n");
	printf("               recursively.\n");
	printf("-s [version]   Sets the bytecode version to compile to. Default is 1.\n");
	printf("-fa [value]    Sets the number of bytes to align functions to. Default is 32.\n");
	printf("-ga [value]    Sets the number of bytes to align globals to. Default is 8.\n");
	printf("-nc            Specifies not to run the compiler.\n");
	printf("-nl            Specifies not to run the linker.\n");
	printf("-d             Indicates debugging symbols should be compiled.\n\n");
	printf("and [files...] include all input files.");
}

void display_version()
{
	printf("LScript Assembler\n");
	printf("Version: %s\n", LSASM_VERSION);
	printf("Build date: %s\n", __DATE__);
	printf("Build time: %s\n", __TIME__);
}

int are_errors(const compile_error_t *list)
{
	while (list)
	{
		if (list->type == error_error)
			return 1;
		list = list->next;
	}
	return 0;
}

input_file_t *add_source_files_in_directory(const char *directory, input_file_t *files, int recursive, const char *unitPrefix)
{
	WIN32_FIND_DATAA ffd;
	CHAR szPrefix[MAX_PATH];
	CHAR szSearchDir[MAX_PATH];
	CHAR szFilePath[MAX_PATH];
	HANDLE hFind = INVALID_HANDLE_VALUE;

	szPrefix[0] = 0;
	if (unitPrefix)
	{
		strcpy_s(szPrefix, sizeof(szPrefix), unitPrefix);
		strcat_s(szPrefix, sizeof(szPrefix), "\\");
	}
	size_t prefixLen = strlen(szPrefix);

	strcpy_s(szSearchDir, sizeof(szSearchDir), directory);
	strcat_s(szSearchDir, sizeof(szSearchDir), "\\*");

	hFind = FindFirstFileA(szSearchDir, &ffd);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		printf("Failed to find directory: %s\n", directory);
		return files;
	}

	do
	{
		strcpy_s(szFilePath, sizeof(szFilePath), directory);
		strcat_s(szFilePath, sizeof(szFilePath), "\\");
		strcat_s(szFilePath, sizeof(szFilePath), ffd.cFileName);

		if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && recursive)
		{
			if (strcmp(ffd.cFileName, ".") && strcmp(ffd.cFileName, ".."))
			{
				strcpy_s(szPrefix + prefixLen, sizeof(szPrefix) - prefixLen, ffd.cFileName);
				files = add_source_files_in_directory(szFilePath, files, 1, szPrefix);
				szPrefix[prefixLen] = 0;
			}
		}
		else
		{
			char *ext = strrchr(ffd.cFileName, '.');
			if (ext && str_equals_ignore_case(ext + 1, "lasm"))
			{
				strcpy_s(szPrefix + prefixLen, sizeof(szPrefix) - prefixLen, ffd.cFileName);
				files = add_file(files, szPrefix, szFilePath);
				szPrefix[prefixLen] = 0;
			}
		}
	} while (FindNextFileA(hFind, &ffd) != 0);

	FindClose(hFind);

	return files;
}
