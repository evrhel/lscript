#pragma once

#include <stdio.h>

typedef struct dump_options_s
{
	const char *infile;		// The input file
	const char *outfile;	// The output file, NULL writes to stdout
	int disasm, symb;		// Disasemble and/or write symbols
} dump_options_t;

enum
{
	dump_no_error = 0x0,
	dump_io_error = 0x1,
	dump_out_of_memory = 0x2,
	dump_not_supported = 0x3
};

/*
* Dump a file.
* 
* @param dumpOptions The options specifying how to dump.
*/
int dump_file(dump_options_t *dumpOptions);