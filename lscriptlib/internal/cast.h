#if !defined(CAST_H)
#define CAST_H

#include "types.h"
#include "datau.h"
#include "lb.h"

typedef int(*cast_proc)(data_t *src, unsigned char castType, data_t *dst);

int handle_cast(void *envPtr, byte_t **executeLocation);

int cast_char(lchar *src, unsigned char castType, data_t *dst);
int cast_uchar(luchar *src, unsigned char castType, data_t *dst);
int cast_short(lshort *src, unsigned char castType, data_t *dst);
int cast_ushort(lushort *src, unsigned char castType, data_t *dst);
int cast_int(lint *src, unsigned char castType, data_t *dst);
int cast_uint(luint *src, unsigned char castType, data_t *dst);
int cast_long(llong *src, unsigned char castType, data_t *dst);
int cast_ulong(lulong *src, unsigned char castType, data_t *dst);
int cast_bool(lbool *src, unsigned char castType, data_t *dst);
int cast_float(lfloat *src, unsigned char castType, data_t *dst);
int cast_double(ldouble *src, unsigned char castType, data_t *dst);

#endif