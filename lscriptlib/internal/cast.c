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
        proc = (cast_proc)cast_char;
        castType = lb_char;
        break;
    case lb_castuc:
        proc = (cast_proc)cast_uchar;
        castType = lb_uchar;
        break;
    case lb_casts:
        proc = (cast_proc)cast_short;
        castType = lb_short;
        break;
    case lb_castus:
        proc = (cast_proc)cast_ushort;
        castType = lb_ushort;
        break;
    case lb_casti:
        proc = (cast_proc)cast_int;
        castType = lb_int;
        break;
    case lb_castui:
        proc = (cast_proc)cast_uint;
        castType = lb_uint;
        break;
    case lb_castl:
        proc = (cast_proc)cast_long;
        castType = lb_long;
        break;
    case lb_castul:
        proc = (cast_proc)cast_ulong;
        castType = lb_ulong;
        break;
    case lb_castb:
        proc = (cast_proc)cast_bool;
        castType = lb_bool;
        break;
    case lb_castf:
        proc = (cast_proc)cast_float;
        castType = lb_float;
        break;
    case lb_castd:
        proc = (cast_proc)cast_double;
        castType = lb_double;
        break;
    default:
        env_raise_exception(env, exception_bad_command, "bad cast");
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
