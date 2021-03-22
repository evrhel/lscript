#if !defined(DEBUG_H)
#define DEBUG_H

typedef struct debug_s debug_t;
typedef struct debug_elem_s debug_elem_t;

struct debug_s
{
	void *buf;
	unsigned int version;
	const char *srcFile;
	debug_elem_t *first;	// May or may not be valid
	debug_elem_t *last;		// May or may not be valid
};

struct debug_elem_s
{
	unsigned int binOff;
	int srcLine;
};

debug_t *load_debug(const char *path);
debug_elem_t *find_debug_elem(debug_t *debug, unsigned int off);
void free_debug(debug_t *debug);

#endif