#if !defined(COLLECTIONS_H)
#define COLLECTIONS_H

enum
{
	error_info,
	error_warning,
	error_error
};

typedef void (*msg_func_t)(const char *const message);


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
	char desc[512];
	compile_error_t *next;
	compile_error_t *front;
	msg_func_t messenger;
};

input_file_t *add_file(input_file_t *back, const char *filename);
void free_file_list(input_file_t *list, int freeData);

compile_error_t *create_base_compile_error(msg_func_t messenger);
compile_error_t *add_compile_error(compile_error_t *back, const char *file, int line, int type, const char *format, ...);
void free_compile_error_list(compile_error_t *front);

#endif