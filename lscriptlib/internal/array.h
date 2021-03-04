#if !defined(ARRAY_H)
#define ARRAY_H

#include "../lscript.h"
#include "datau.h"

#define ARRAY_INDEX_INBOUNDS(array, index) ((index)<(array)->length)
#define ARRAY_GET_VALUE(array, type, index) (((type*)&(array)->data)[(index)])
#define ARRAY_SET_VALUE(array, type, index, value) ((((type*)&(array)->data)[(index)])=(value))

typedef struct array_s array_t;
struct array_s
{
	signed long long flags;
	unsigned int length;
	unsigned int dummy;
	void *data;
};

inline data_t *array_get_data(array_t *array, luint index, size_t elemSize)
{
	return (data_t *)((byte_t *)&array->data + (index * elemSize));
}

inline lchar array_get_char(array_t *array, luint index)
{
	return ARRAY_GET_VALUE(array, lchar, index);
}

inline luchar array_get_uchar(array_t *array, luint index)
{
	return ARRAY_GET_VALUE(array, luchar, index);
}

inline lshort array_get_short(array_t *array, luint index)
{
	return ARRAY_GET_VALUE(array, lshort, index);
}

inline lushort array_get_ushort(array_t *array, luint index)
{
	return ARRAY_GET_VALUE(array, lushort, index);
}

inline lint array_get_int(array_t *array, luint index)
{
	return ARRAY_GET_VALUE(array, lint, index);
}

inline luint array_get_uint(array_t *array, luint index)
{
	return ARRAY_GET_VALUE(array, luint, index);
}

inline llong array_get_long(array_t *array, luint index)
{
	return ARRAY_GET_VALUE(array, llong, index);
}

inline lulong array_get_ulong(array_t *array, luint index)
{
	return ARRAY_GET_VALUE(array, lulong, index);
}

inline lbool array_get_bool(array_t *array, luint index)
{
	return ARRAY_GET_VALUE(array, lbool, index);
}

inline lfloat array_get_float(array_t *array, luint index)
{
	return ARRAY_GET_VALUE(array, lfloat, index);
}

inline ldouble array_get_double(array_t *array, luint index)
{
	return ARRAY_GET_VALUE(array, ldouble, index);
}

inline lobject array_get_object(array_t *array, luint index)
{
	return ARRAY_GET_VALUE(array, lobject, index);
}

inline void array_set_char(array_t *array, luint index, lchar value)
{
	ARRAY_SET_VALUE(array, lchar, index, value);
}

inline void array_set_uchar(array_t *array, luint index, luchar value)
{
	ARRAY_SET_VALUE(array, luchar, index, value);
}

inline void array_set_short(array_t *array, luint index, lshort value)
{
	ARRAY_SET_VALUE(array, lshort, index, value);
}

inline void array_set_ushort(array_t *array, luint index, lushort value)
{
	ARRAY_SET_VALUE(array, lushort, index, value);
}

inline void array_set_int(array_t *array, luint index, lint value)
{
	ARRAY_SET_VALUE(array, lint, index, value);
}

inline void array_set_uint(array_t *array, luint index, luint value)
{
	ARRAY_SET_VALUE(array, luint, index, value);
}

inline void array_set_long(array_t *array, luint index, llong value)
{
	ARRAY_SET_VALUE(array, llong, index, value);
}

inline void array_set_ulong(array_t *array, luint index, lulong value)
{
	ARRAY_SET_VALUE(array, lulong, index, value);
}

inline void array_set_bool(array_t *array, luint index, lbool value)
{
	ARRAY_SET_VALUE(array, lbool, index, value);
}

inline void array_set_float(array_t *array, luint index, lfloat value)
{
	ARRAY_SET_VALUE(array, lfloat, index, value);
}

inline void array_set_double(array_t *array, luint index, ldouble value)
{
	ARRAY_SET_VALUE(array, ldouble, index, value);
}

inline void array_set_object(array_t *array, luint index, lobject value)
{
	ARRAY_SET_VALUE(array, lobject, index, value);
}

#endif