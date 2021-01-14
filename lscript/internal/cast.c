#include "cast.h"

#define CAST(src, castType, dst) (dst=(castType)src)

/*
This macro is kinda gross, but whatever it saves typing
*/
#define CAST_GENERIC(src, castType, dst) \
switch (castType) \
{\
    case lb_char:\
        dst->cvalue = (lchar)*src;\
        break;\
    case lb_uchar:\
        dst->cvalue = (luchar)*src;\
        break;\
    case lb_short:\
        dst->svalue = (lshort)*src;\
        break;\
    case lb_ushort:\
        dst->usvalue = (lushort)*src;\
        break;\
    case lb_int:\
        dst->ivalue = (lint)*src;\
        break;\
    case lb_uint:\
        dst->uivalue = (luint)*src;\
        break;\
    case lb_long:\
        dst->lvalue = (llong)*src;\
        break;\
    case lb_ulong:\
        dst->ulvalue = (lulong)*src;\
        break;\
    case lb_bool:\
        dst->bvalue = (lbool)(*src ? 1 : 0);\
        break;\
    case lb_float:\
        dst->fvalue = (lfloat)*src;\
        break;\
    case lb_double:\
        dst->dvalue = (ldouble)*src;\
        break;\
    default:\
        return 0;\
}\
return 1;\

int cast_char(lchar *src, unsigned char castType, data_t *dst)
{
    CAST_GENERIC(src, castType, dst);
}

int cast_uchar(luchar *src, unsigned char castType, data_t *dst)
{
    CAST_GENERIC(src, castType, dst);
}

int cast_short(lshort *src, unsigned char castType, data_t *dst)
{
    CAST_GENERIC(src, castType, dst);
}

int cast_ushort(lushort *src, unsigned char castType, data_t *dst)
{
    CAST_GENERIC(src, castType, dst);
}

int cast_int(lint *src, unsigned char castType, data_t *dst)
{
    CAST_GENERIC(src, castType, dst);
}

int cast_uint(luint *src, unsigned char castType, data_t *dst)
{
    CAST_GENERIC(src, castType, dst);
}

int cast_long(llong *src, unsigned char castType, data_t *dst)
{
    CAST_GENERIC(src, castType, dst);
}

int cast_ulong(lulong *src, unsigned char castType, data_t *dst)
{
    CAST_GENERIC(src, castType, dst);
}

int cast_bool(lbool *src, unsigned char castType, data_t *dst)
{
    CAST_GENERIC(src, castType, dst);
}

int cast_float(lfloat *src, unsigned char castType, data_t *dst)
{
    CAST_GENERIC(src, castType, dst);
}

int cast_double(ldouble *src, unsigned char castType, data_t *dst)
{
    CAST_GENERIC(src, castType, dst);
}
