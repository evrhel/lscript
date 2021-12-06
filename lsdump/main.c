#include <strutil/strutil.h>
#include <stdlib.h>
#include <memory.h>

#include "dump.h"

#define LSDUMP_VERSION "1.0.0"

static void display_help();
static void display_version();

int main(int argc, char *argv[])
{
	dump_options_t options;
	memset(&options, 0, sizeof(options));

	for (int i = 1; i < argc; i++)
	{
		if (argv[i][0] != '-')
		{
			if (i < argc - 1)
			{
				display_help();
				return 0;
			}
			else options.infile = argv[i];
		}
		else if (str_equals_ignore_case(argv[i], "-help") || str_equals_ignore_case(argv[i], "-h") || str_equals_ignore_case(argv[i], "-?"))
		{
			display_help();
			return 0;
		}
		else if (str_equals_ignore_case(argv[i], "-version") || str_equals_ignore_case(argv[i], "-v"))
		{
			display_version();
			return 0;
		}
		else if (str_equals_ignore_case(argv[i], "-o"))
		{
			i++;
			if (i == argc)
			{
				display_help();
				return 0;
			}
			options.outfile = argv[i];
		}
		else if (str_equals_ignore_case(argv[i], "-d"))
		{
			options.disasm = 1;
		}
		else if (str_equals_ignore_case(argv[i], "-s"))
		{
			options.symb = 1;
		}
		else
		{
			printf("Unknown switch: %s\n", argv[i]);
			display_help();
		}
	}

	if (!options.infile)
	{
		printf("Must specify input file.\n");
		display_help();
		return 0;
	}

	if (!options.disasm && !options.symb)
	{
		printf("Must specify to disassemble and/or display symbols\n");
		display_help();
		return 0;
	}

	int err = dump_file(&options);
	switch (err)
	{
	case dump_no_error:
		break;
	case dump_io_error:
		printf("IO error\n");
		break;
	case dump_out_of_memory:
		printf("Out of memory\n");
		break;
	}

	return 0;
}

void display_help()
{
	printf("LScript Dump Utility\n");
	printf("Usage: lsdump [options...] [file]\n\n");
	printf("Where [options...] include:\n");
	printf("-help -h -?    Prints this help message.\n");
	printf("-version -v    Displays version information.\n");
	printf("-o [directory] Optionally specifies a file to dump to. If not specified,\n");
	printf("               information will be written to stdout.\n");
	printf("-d             Specifies to disassemble the input file.\n");
	printf("-s             Specifies to display all public symbols in the input file.\n");
	printf("and [file] is the file to operate on.");
}

void display_version()
{
	printf("LScript Dump Utility\n");
	printf("Version: %s\n", LSDUMP_VERSION);
	printf("Build date: %s\n", __DATE__);
	printf("Build time: %s\n", __TIME__);
}