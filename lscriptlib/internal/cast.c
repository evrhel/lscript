#include "cast.h"

#include "vm.h"

#define CAST(src, castType, dst) (dst=(castType)src)

/*
Generically casts some value into a destination data_t
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
return 1;

int handle_cast(void *envPtr, byte_t **executeLocation)
{
    env_t *env = (env_t *)envPtr;

    cast_proc proc;
    unsigned int castType;

    switch (**executeLocation)
    {
    case lb_castc:
        castType = lb_char;
        break;
    case lb_castuc:
        castType = lb_uchar;
        break;
    case lb_casts:
        castType = lb_short;
        break;
    case lb_castus:
        castType = lb_ushort;
        break;
    case lb_casti:
        castType = lb_int;
        break;
    case lb_castui:
        castType = lb_uint;
        break;
    case lb_castl:
        castType = lb_long;
        break;
    case lb_castul:
        castType = lb_ulong;
        break;
    case lb_castb:
        castType = lb_bool;
        break;
    case lb_castf:
        castType = lb_float;
        break;
    case lb_castd:
        castType = lb_double;
        break;
    default:
        env_raise_exception(env, exception_bad_command, "bad cast type");
        return 0;
        break;
    }

    (*executeLocation)++;

    char *dstName = (char *)(*executeLocation);
    data_t *dstData;
    flags_t dstFlags;

    (*executeLocation) += strlen(*executeLocation) + 1;

    char *srcName = (char *)(*executeLocation);
    data_t *srcData;
    flags_t srcFlags;

    (*executeLocation) += strlen(*executeLocation) + 1;

    if (!env_resolve_variable(env, dstName, &dstData, &dstFlags))
        return 0;

    if (!env_resolve_variable(env, srcName, &srcData, &srcFlags))
        return 0;

    switch (TYPEOF(srcFlags))
    {
    case lb_char:
        proc = cast_char;
        break;
    case lb_uchar:
        proc = cast_uchar;
        break;
    case lb_short:
        proc = cast_short;
        break;
    case lb_ushort:
        proc = cast_ushort;
        break;
    case lb_int:
        proc = cast_int;
        break;
    case lb_uint:
        proc = cast_uint;
        break;
    case lb_long:
        proc = cast_long;
        break;
    case lb_ulong:
        proc = cast_ulong;
        break;
    case lb_bool:
        proc = cast_bool;
        break;
    case lb_float:
        proc = cast_float;
        break;
    case lb_double:
        proc = cast_double;
        break;
    default:
        env_raise_exception(env, exception_bad_command, "bad cast source variable");
        return 0;
        break;
    }

    return proc(srcData, castType, dstData);
}

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
