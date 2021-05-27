#if !defined(TYPES_H)
#define TYPES_H

#if defined(_WIN32)
#include <Windows.h>

typedef BYTE byte_t;
typedef WORD word_t;
typedef DWORD dword_t;
typedef DWORDLONG qword_t;
typedef FLOAT real4_t;
typedef DOUBLE real8_t;

#else
typedef unsigned char byte_t;
typedef unsigned short word_t;
typedef unsigned int dword_t;
typedef unsigned long long qword_t;
typedef float real4_t;
typedef double real8_t;
#endif

typedef unsigned long long flags_t;

#endif
