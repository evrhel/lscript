#if !defined(VALUE_H)
#define VALUE_H

#include "../lscript.h"
#include "lb.h"
#include "types.h"

#define VALUE_STATIC_OFFSET 0
#define VALUE_CONST_OFFSET 1
#define VALUE_MANAGER_FLAGS_OFFSET 6
#define VALUE_TYPE_OFFSET 7

#define TYPEOF(F) value_typeof((value_t*)&(F))
#define SET_TYPE(F, T) value_set_type((value_t*)&(F),(T))

typedef struct value_s value_t;
struct value_s
{
	flags_t flags;
	union
	{
		lchar cvalue;
		luchar ucvalue;
		lshort svalue;
		lushort usvalue;
		lint ivalue;
		luint uivalue;
		llong lvalue;
		lulong ulvalue;
		lbool bvalue;
		lfloat fvalue;
		ldouble dvalue;
		lobject ovalue;

		lchararray cavalue;
		luchararray ucavalue;
		lshortarray savalue;
		lushortarray usavalue;
		lintarray iavalue;
		luintarray uiavalue;
		llongarray lavalue;
		lulongarray ulavalue;
		lboolarray bavalue;
		lobjectarray oavalue;
	};
};

typedef struct field_s field_t;
struct field_s
{
	flags_t flags;
	void *offset;
};

inline char value_is_static(const value_t *value)
{
	return *((byte_t *)(&value->flags) + VALUE_STATIC_OFFSET) == lb_static;
}

inline char value_is_const(const value_t *value)
{
	return *((byte_t *)(&value->flags) + VALUE_CONST_OFFSET) == lb_const;
}

inline byte_t *value_manager_flags(const value_t *value)
{
	return ((byte_t *)(&value->flags) + VALUE_MANAGER_FLAGS_OFFSET);
}

inline byte_t value_typeof(const value_t *value)
{
	return *((byte_t *)(&value->flags) + VALUE_TYPE_OFFSET);
}

inline void value_set_type(const value_t *value, byte_t type)
{
	*((byte_t *)(&value->flags) + VALUE_TYPE_OFFSET) = type;
}

inline char field_is_static(const field_t *field)
{
	return value_is_static((const value_t *)field);
}

inline char field_is_const(const field_t *field)
{
	return value_is_const((const value_t *)field);
}

inline char field_typeof(const field_t *field)
{
	return value_typeof((const value_t *)field);
}


inline size_t sizeof_type(byte_t type)
{
	switch (type)
	{
	case lb_char:
	case lb_uchar:
		return sizeof(lchar);
	case lb_short:
	case lb_ushort:
		return sizeof(lshort);
	case lb_int:
	case lb_uint:
		return sizeof(lint);
	case lb_long:
	case lb_ulong:
		return sizeof(llong);
	case lb_bool:
		return sizeof(lbool);
	case lb_float:
		return sizeof(lfloat);
	case lb_double:
		return sizeof(ldouble);
	case lb_object:
		return sizeof(lobject);
	case lb_chararray:
	case lb_uchararray:
	case lb_shortarray:
	case lb_ushortarray:
	case lb_intarray:
	case lb_uintarray:
	case lb_longarray:
	case lb_ulongarray:
	case lb_boolarray:
	case lb_objectarray:
		return sizeof(void *);
	default:
		return 0;
	}
}

inline size_t value_sizeof(const value_t *value)
{
	return sizeof_type(value_typeof(value));
}

#endif