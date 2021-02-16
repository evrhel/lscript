#include "object.h"

#include "value.h"

static inline value_t *value_at(object_t *object, const char *fieldName)
{
	return (value_t *)((char *)(&object->data) + (size_t)object_get_field_data(object, fieldName)->offset - 8);
}

void object_set_char(object_t *object, const char *fieldName, lchar value)
{
	value_at(object, fieldName)->cvalue = value;
}

void object_set_uchar(object_t *object, const char *fieldName, luchar value)
{
	value_at(object, fieldName)->ucvalue = value;
}

void object_set_short(object_t *object, const char *fieldName, lshort value)
{
	value_at(object, fieldName)->svalue = value;
}

void object_set_ushort(object_t *object, const char *fieldName, lushort value)
{
	value_at(object, fieldName)->usvalue = value;
}

void object_set_int(object_t *object, const char *fieldName, lint value)
{
	value_at(object, fieldName)->ivalue = value;
}

void object_set_uint(object_t *object, const char *fieldName, luint value)
{
	value_at(object, fieldName)->uivalue = value;
}

void object_set_long(object_t *object, const char *fieldName, llong value)
{
	value_at(object, fieldName)->lvalue = value;
}

void object_set_ulong(object_t *object, const char *fieldName, lulong value)
{
	value_at(object, fieldName)->ulvalue = value;
}

void object_set_bool(object_t *object, const char *fieldName, lbool value)
{
	value_at(object, fieldName)->bvalue = value;
}

void object_set_float(object_t *object, const char *fieldName, lfloat value)
{
	value_at(object, fieldName)->fvalue = value;
}

void object_set_double(object_t *object, const char *fieldName, ldouble value)
{
	value_at(object, fieldName)->dvalue = value;
}

void object_set_object(object_t *object, const char *fieldName, lobject value)
{
	value_at(object, fieldName)->ovalue = value;
}

lchar object_get_char(object_t *object, const char *fieldName)
{
	return value_at(object, fieldName)->cvalue;
}

luchar object_get_uchar(object_t *object, const char *fieldName)
{
	return value_at(object, fieldName)->ucvalue;
}

lshort object_get_short(object_t *object, const char *fieldName)
{
	return value_at(object, fieldName)->svalue;
}

lushort object_get_ushort(object_t *object, const char *fieldName)
{
	return value_at(object, fieldName)->usvalue;
}

lint object_get_int(object_t *object, const char *fieldName)
{
	return value_at(object, fieldName)->ivalue;
}

luint object_get_uint(object_t *object, const char *fieldName)
{
	return value_at(object, fieldName)->uivalue;
}

llong object_get_long(object_t *object, const char *fieldName)
{
	return value_at(object, fieldName)->lvalue;
}

lulong object_get_ulong(object_t *object, const char *fieldName)
{
	return value_at(object, fieldName)->ulvalue;
}

lbool object_get_bool(object_t *object, const char *fieldName)
{
	return value_at(object, fieldName)->bvalue;
}

lfloat object_get_float(object_t *object, const char *fieldName)
{
	return value_at(object, fieldName)->fvalue;
}

ldouble object_get_double(object_t *object, const char *fieldName)
{
	return value_at(object, fieldName)->dvalue;
}

lobject object_get_object(object_t *object, const char *fieldName)
{
	return value_at(object, fieldName)->ovalue;
}
