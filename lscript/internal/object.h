#if !defined(OBJECT_H)
#define OBJECT_H

#include "class.h"
#include "../lscript.h"

typedef struct object_s object_t;
struct object_s
{
	signed long long buffer;
	class_t *clazz;
	void *data;
};

inline field_t *object_get_field_data(const object_t *object, const char *fieldName)
{
	return (field_t *)map_at(object->clazz->fields, fieldName);
}

inline unsigned char object_get_field_type(const object_t *object, const char *fieldName)
{
	return field_typeof(object_get_field_data(object, fieldName));
}

void object_set_char(object_t *object, const char *fieldName, lchar value);
void object_set_uchar(object_t *object, const char *fieldName, luchar value);
void object_set_short(object_t *object, const char *fieldName, lshort value);
void object_set_ushort(object_t *object, const char *fieldName, lushort value);
void object_set_int(object_t *object, const char *fieldName, lint value);
void object_set_uint(object_t *object, const char *fieldName, luint value);
void object_set_long(object_t *object, const char *fieldName, llong value);
void object_set_ulong(object_t *object, const char *fieldName, lulong value);
void object_set_bool(object_t *object, const char *fieldName, lbool value);
void object_set_float(object_t *object, const char *fieldName, lfloat value);
void object_set_double(object_t *object, const char *fieldName, ldouble value);
void object_set_object(object_t *object, const char *fieldName, lobject value);

lchar object_get_char(object_t *object, const char *fieldName);
luchar object_get_uchar(object_t *object, const char *fieldName);
lshort object_get_short(object_t *object, const char *fieldName);
lushort object_get_ushort(object_t *object, const char *fieldName);
lint object_get_int(object_t *object, const char *fieldName);
luint object_get_uint(object_t *object, const char *fieldName);
llong object_get_long(object_t *object, const char *fieldName);
lulong object_get_ulong(object_t *object, const char *fieldName);
lbool object_get_bool(object_t *object, const char *fieldName);
lfloat object_get_float(object_t *object, const char *fieldName);
ldouble object_get_double(object_t *object, const char *fieldName);
lobject object_get_object(object_t *object, const char *fieldName);

#endif