#if !defined(COMPILER_H)
#define COMPILER_H

enum
{
	error_info,
	error_warning,
	error_error
};

typedef struct input_file_s input_file_t;
struct input_file_s
{
	const char *filename;
	input_file_t *next;
	input_file_t *front;
};

typedef struct compile_error_s compile_error_t;
struct compile_error_s
{
	const char *file;
	int line, type;
	const char *desc;
	compile_error_t *next;
	compile_error_t *front;
};

input_file_t *add_file(input_file_t *front, const char *filename);
void free_file_list(input_file_t *list);

compile_error_t *add_compile_error(compile_error_t *front, const char *file, int line, int type, const char *desc);
void free_compile_error_list(compile_error_t *front);

compile_error_t *compile(input_file_t *files, const char *outputDirectory);

#endif