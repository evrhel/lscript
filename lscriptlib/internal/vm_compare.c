#include "vm_compare.h"

#include "vm.h"

#define TOBOOL(intExpr) ((intExpr)?1:0)

#define PUTFLAGS(lhsData, rhsData, resultPtr) \
if ((lhsData) == (rhsData)) \
(*(resultPtr)) |= (compare_equal | compare_greaterequ | compare_lessequ); \
else if ((lhsData) > (rhsData)) \
(*(resultPtr))|= (compare_greater | compare_greaterequ); \
else \
(*(resultPtr)) |= (compare_less | compare_lessequ);


#define SUBCOMPARE(lhsData, rhs, rhf, resultPtr) \
switch (TYPEOF(rhf)) \
{ \
case lb_char: \
PUTFLAGS(lhsData, (rhs)->cvalue, resultPtr); \
break; \
case lb_uchar: \
PUTFLAGS(lhsData, (rhs)->ucvalue, resultPtr); \
break; \
case lb_short: \
PUTFLAGS(lhsData, (rhs)->svalue, resultPtr); \
break; \
case lb_ushort: \
PUTFLAGS(lhsData, (rhs)->usvalue, resultPtr); \
break; \
case lb_int: \
PUTFLAGS(lhsData, (rhs)->ivalue, resultPtr); \
break; \
case lb_uint: \
PUTFLAGS(lhsData, (rhs)->uivalue, resultPtr); \
break; \
case lb_long: \
PUTFLAGS(lhsData, (rhs)->lvalue, resultPtr); \
break; \
case lb_ulong: \
PUTFLAGS(lhsData, (rhs)->ulvalue, resultPtr); \
break; \
case lb_float: \
PUTFLAGS(lhsData, (rhs)->fvalue, resultPtr); \
break; \
case lb_double: \
PUTFLAGS(lhsData, (rhs)->dvalue, resultPtr); \
break; \
case lb_bool: \
PUTFLAGS(lhsData, (rhs)->bvalue, resultPtr); \
break; \
}

#define COMPARE(lhs, lhf, rhs, rhf, resultPtr) \
switch (TYPEOF(lhf)) \
{ \
case lb_char: \
SUBCOMPARE((lhs)->cvalue, rhs, rhf, resultPtr); \
break; \
case lb_uchar: \
SUBCOMPARE((lhs)->ucvalue, rhs, rhf, resultPtr); \
break; \
case lb_short: \
SUBCOMPARE((lhs)->svalue, rhs, rhf, resultPtr); \
break; \
case lb_ushort: \
SUBCOMPARE((lhs)->usvalue, rhs, rhf, resultPtr); \
break; \
case lb_int: \
SUBCOMPARE((lhs)->ivalue, rhs, rhf, resultPtr); \
break; \
case lb_uint: \
SUBCOMPARE((lhs)->uivalue, rhs, rhf, resultPtr); \
break; \
case lb_long: \
SUBCOMPARE((lhs)->lvalue, rhs, rhf, resultPtr); \
break; \
case lb_ulong: \
SUBCOMPARE((lhs)->ulvalue, rhs, rhf, resultPtr); \
break; \
case lb_float: \
SUBCOMPARE((lhs)->fvalue, rhs, rhf, resultPtr); \
break; \
case lb_double: \
SUBCOMPARE((lhs)->dvalue, rhs, rhf, resultPtr); \
break; \
case lb_bool: \
SUBCOMPARE((lhs)->bvalue, rhs, rhf, resultPtr); \
break; \
};

static int resolve_data(env_t *env, byte_t **counterPtr, data_t **data, flags_t *flags);

int vmc_compare(void *envPtr, byte_t **counterPtr)
{
    env_t *env = (env_t *)envPtr;

    data_t *lhs, *rhs;
    flags_t lhf, rhf;

    (*counterPtr)++;
    byte_t count = **counterPtr;
    (*counterPtr)++;

    if (!resolve_data(env, counterPtr, &lhs, &lhf))
        return 0;

    if (count == lb_one)
    {
        (*counterPtr)++;
        return lhs->bvalue;
    }
    else if (count == lb_two)
    {
        byte_t comparator = **counterPtr;
        (*counterPtr)++;

        if (!resolve_data(env, counterPtr, &rhs, &rhf))
            return 0;

        int result = 0;
        COMPARE(lhs, lhf, rhs, rhf, &result);

        switch (comparator)
        {
        case lb_equal:
            return TOBOOL(result & compare_equal);
            break;
        case lb_nequal:
            return TOBOOL(!(result & compare_equal));
            break;
        case lb_greater:
            return TOBOOL(result & compare_greater);
            break;
        case lb_gequal:
            return TOBOOL(result & compare_greaterequ);
            break;
        case lb_less:
            return TOBOOL(result & compare_less);
            break;
        case lb_lequal:
            return TOBOOL(result & compare_lessequ);
            break;
        default:
            env_raise_exception(env, exception_bad_command, "invalid comparator %x", (unsigned int)comparator);
            return 0;
        }
    }
    else
    {
        env_raise_exception(env, exception_bad_command, "invalid compare count constant %x", (unsigned int)count);
        return 0;
    }
}

int vmc_compare_data(data_t *lhs, flags_t lhf, data_t *rhs, flags_t rhf)
{
    int result = 0;
    COMPARE(lhs, lhf, rhs, rhs, &result);
    return result;
}

int resolve_data(env_t *env, byte_t **counterPtr, data_t **data, flags_t *flags)
{
    byte_t *counter = *counterPtr;
    byte_t *dataStart = ++counter;
    *flags = 0;
    switch (*(counter - 1))
    {
    case lb_char:
        SET_TYPE(*flags, lb_char);
        counter += sizeof(lchar);
        break;
    case lb_uchar:
        SET_TYPE(*flags, lb_uchar);
        counter += sizeof(luchar);
        break;
    case lb_short:
        SET_TYPE(*flags, lb_short);
        counter += sizeof(lshort);
        break;
    case lb_ushort:
        SET_TYPE(*flags, lb_ushort);
        counter += sizeof(lushort);
        break;
    case lb_int:
        SET_TYPE(*flags, lb_int);
        counter += sizeof(lint);
        break;
    case lb_uint:
        SET_TYPE(*flags, lb_uint);
        counter += sizeof(luint);
        break;
    case lb_long:
        SET_TYPE(*flags, lb_long);
        counter += sizeof(llong);
        break;
    case lb_ulong:
        SET_TYPE(*flags, lb_ulong);
        counter += sizeof(lulong);
        break;
    case lb_float:
        SET_TYPE(*flags, lb_float);
        counter += sizeof(lfloat);
        break;
    case lb_double:
        SET_TYPE(*flags, lb_double);
        counter += sizeof(ldouble);
        break;
    case lb_bool:
        SET_TYPE(*flags, lb_bool);
        counter += sizeof(lbool);
        break;
    case lb_value:
        if (!env_resolve_variable(env, (const char *)counter, data, flags))
            return 0;
        counter += strlen(counter) + 1;
        *counterPtr = counter;
        return 1;
        break;
    default:
        env_raise_exception(env, exception_bad_command, "bad compare data type %x", (unsigned int)(*(counter - 1)));
        return 0;
    }
    *data = (data_t *)dataStart;
    *counterPtr = counter;
    return 1;
}
