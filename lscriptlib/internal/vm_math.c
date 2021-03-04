#include "vm_math.h"

#include "cast.h"
#include <string.h>
#include <math.h>

#define FADD(a, b) a + b
#define FSUB(a, b) a - b
#define FMUL(a, b) a * b
#define FDIV(a, b) a / b

/*
Tries to fetch the next named variable stored in the add command, throwing a vm
exception and returning 0 if it fails.

@param env A pointer to a valid env_t structure
@param argLocPtrPtr A pointer to a pointer of some data containing the add command at the offset of the argument field
@param nextNameStringPtr A pointer to a string which will contain the name of the variable fetched
@param dataPtrPtr A pointer to a pointer to a data_t where the argument data will be stored
@param typePtr A pointer to where the type of the data will be stored
@param flagPtr A pointer to a flags_t where the data's flags will be stored
*/
#define TRY_FETCH_NEXT_VAR(env, argLocPtrPtr, nextNameStringPtr, dataPtrPtr, typePtr, flagPtr) \
*nextNameStringPtr = (const char *)*argLocPtrPtr; \
if (!env_resolve_variable(env, *nextNameStringPtr, dataPtrPtr, flagPtr)) return 0; \
*typePtr = TYPEOF(*flagPtr); \
*argLocPtrPtr += strlen(*nextNameStringPtr) + 1;

/*
Tries to fetch the argument field data of an add command, throwing a vm
exception and returning 0 if it fails.

@param env A pointer to a valid env_t structure
@param argLocPtrPtr A pointer to a pointer of some data containing the add command at the offset of the argument field
@param argTypePtr A pointer to where the type of the argument will be stored
@param argDataPtrPtr A pointer to a pointer to a data_t where the argument data will be stored
@param argFlagsPtr A pointer to a flags_t where the argument's flags will be stored
*/
#define TRY_FETCH_ARG_DATA(env, argLocPtrPtr, argTypePtr, argDataPtrPtr, argFlagsPtr) \
*argTypePtr = **argLoc; \
(*argLoc)++; \
*argDataPtrPtr = (data_t *)*argLocPtrPtr; \
switch (*argTypePtr) \
{ \
case lb_char: \
case lb_uchar: \
	*argLoc += sizeof(lchar); \
	break; \
case lb_short: \
case lb_ushort: \
	*argLoc += sizeof(lshort); \
	break; \
case lb_int: \
case lb_uint: \
	*argLoc += sizeof(lint); \
	break; \
case lb_long: \
case lb_ulong: \
	*argLoc += sizeof(llong); \
	break; \
case lb_float: \
	*argLoc += sizeof(lfloat); \
	break; \
case lb_double: \
	*argLoc += sizeof(ldouble); \
	break; \
case lb_value: \
{ \
	const char *__argName = (const char *)*argLoc; \
	if (!env_resolve_variable(env, __argName, argDataPtrPtr, argFlagsPtr)) \
		return 0; \
	*argLoc += strlen(__argName) + 1; \
	*argTypePtr = TYPEOF(*argFlagsPtr); \
} \
	break; \
default: \
	env->exception = exception_bad_command; \
	return 0; \
	break; \
}

/*
Cast data of some type into a result pointer

@param srcDataPtr A pointer to the source data_t
@param srcDataType The type stored in srcDataPtr
@param castType The type to cast to
@param resultPtr A pointer to the destination data_t
*/
#define CAST(srcDataPtr, srcDataType, castType, resultPtr) \
switch (srcDataType) \
{ \
case lb_char: \
	cast_char(&srcDataPtr->cvalue, castType, resultPtr); \
	break; \
case lb_uchar: \
	cast_uchar(&srcDataPtr->ucvalue, castType, resultPtr); \
	break; \
case lb_short: \
	cast_short(&srcDataPtr->svalue, castType, resultPtr); \
	break; \
case lb_ushort: \
	cast_ushort(&srcDataPtr->usvalue, castType, resultPtr); \
	break; \
case lb_int: \
	cast_int(&srcDataPtr->ivalue, castType, resultPtr); \
	break; \
case lb_uint: \
	cast_uint(&srcDataPtr->uivalue, castType, resultPtr); \
	break; \
case lb_long: \
	cast_long(&srcDataPtr->lvalue, castType, resultPtr); \
	break; \
case lb_ulong: \
	cast_ulong(&srcDataPtr->ulvalue, castType, resultPtr); \
	break; \
case lb_float: \
	cast_float(&srcDataPtr->fvalue, castType, resultPtr); \
	break; \
case lb_double: \
	cast_double(&srcDataPtr->dvalue, castType, resultPtr); \
	break; \
}

/*
Performs the requested operation on two values of the same type.

@param dstDataPtr A pointer to the data_t where the result will be stored
@param srcDataPtr A pointer to the source data_t
@param argDataPtr A pointer to the second data_t
@param dataType The type of data to perform the operation on
@param op The operation to perform (dst = src [op] arg)
@param flopf The special floating point operation function for floats to use
@param flopd The special floating point operation function for doubles to use
*/
#define DO_OP(dstDataPtr, srcDataPtr, argDataPtr, dataType, op, flopf, flopd) \
switch (dataType) \
{ \
case lb_char: \
	(dstDataPtr)->cvalue = (srcDataPtr)->cvalue op (argDataPtr)->cvalue; \
	break; \
case lb_uchar: \
	(dstDataPtr)->ucvalue = (srcDataPtr)->ucvalue op (argDataPtr)->ucvalue; \
	break; \
case lb_short: \
	(dstDataPtr)->svalue = (srcDataPtr)->svalue op (argDataPtr)->svalue; \
	break; \
case lb_ushort: \
	(dstDataPtr)->usvalue = (srcDataPtr)->usvalue op (argDataPtr)->usvalue; \
	break; \
case lb_int: \
	(dstDataPtr)->ivalue = (srcDataPtr)->ivalue op (argDataPtr)->ivalue; \
	break; \
case lb_uint: \
	(dstDataPtr)->uivalue = (srcDataPtr)->uivalue op (argDataPtr)->uivalue; \
	break; \
case lb_long: \
	(dstDataPtr)->lvalue = (srcDataPtr)->lvalue op (argDataPtr)->lvalue; \
	break; \
case lb_ulong: \
	(dstDataPtr)->ulvalue = (srcDataPtr)->ulvalue op (argDataPtr)->ulvalue; \
	break; \
case lb_float: \
	(dstDataPtr)->fvalue = flopf((srcDataPtr)->fvalue, (argDataPtr)->fvalue); \
	break; \
case lb_double: \
	(dstDataPtr)->dvalue = flopd((srcDataPtr)->dvalue, (argDataPtr)->dvalue); \
	break; \
}

int vmm_add(env_t *env, byte_t **argLoc)
{
	const char *srcName = (const char *)argLoc;
	const char *dstName;
	data_t *srcData, *argData, *dstData;
	flags_t srcFlags, argFlags, dstFlags;
	byte_t srcType, argType, dstType;
	data_t srcCast, argCast;

	// Get the destination variable and its type
	TRY_FETCH_NEXT_VAR(env, argLoc, &dstName, &dstData, &dstType, &dstFlags);

	// Get the source variable and its type
	TRY_FETCH_NEXT_VAR(env, argLoc, &srcName, &srcData, &srcType, &srcFlags);

	// Get the type of the next argument in the add command and fetch the data stored
	// there, incrementing by the required number of bytes.
	TRY_FETCH_ARG_DATA(env, argLoc, &argType, &argData, &argFlags);

	// Cast the source value to the destination type
	CAST(srcData, srcType, dstType, &srcCast);

	// Cast the argument value to the destination type
	CAST(argData, argType, dstType, &argCast);

	// Perform the addition, placing the result in the destination variable
	DO_OP(dstData, &srcCast, &argCast, dstType, +, FADD, FADD);

	return 1;
}

int vmm_sub(env_t *env, byte_t **argLoc)
{
	const char *srcName = (const char *)argLoc;
	const char *argName;
	const char *dstName;
	data_t *srcData, *argData, *dstData;
	flags_t srcFlags, argFlags, dstFlags;
	byte_t srcType, argType, dstType;
	data_t srcCast, argCast;

	// Get the destination variable and its type
	TRY_FETCH_NEXT_VAR(env, argLoc, &dstName, &dstData, &dstType, &dstFlags);

	// Get the source variable and its type
	TRY_FETCH_NEXT_VAR(env, argLoc, &srcName, &srcData, &srcType, &srcFlags);

	// Get the type of the next argument in the sub command and fetch the data stored
	// there, incrementing by the required number of bytes.
	TRY_FETCH_ARG_DATA(env, argLoc, &argType, &argData, &argFlags);

	// Cast the source value to the destination type
	CAST(srcData, srcType, dstType, &srcCast);

	// Cast the argument value to the destination type
	CAST(argData, argType, dstType, &argCast);

	// Perform the subtraction, placing the result in the destination variable
	DO_OP(dstData, &srcCast, &argCast, dstType, -, FSUB, FSUB);

	return 1;
}

int vmm_mul(env_t *env, byte_t **argLoc)
{
	const char *srcName = (const char *)argLoc;
	//const char *argName;
	const char *dstName;
	data_t *srcData, *argData, *dstData;
	flags_t srcFlags, argFlags, dstFlags;
	byte_t srcType, argType, dstType;
	data_t srcCast, argCast;

	// Get the destination variable and its type
	TRY_FETCH_NEXT_VAR(env, argLoc, &dstName, &dstData, &dstType, &dstFlags);

	// Get the source variable and its type
	TRY_FETCH_NEXT_VAR(env, argLoc, &srcName, &srcData, &srcType, &srcFlags);

	// Get the type of the next argument in the mul command and fetch the data stored
	// there, incrementing by the required number of bytes.
	TRY_FETCH_ARG_DATA(env, argLoc, &argType, &argData, &argFlags);

	// Cast the source value to the destination type
	CAST(srcData, srcType, dstType, &srcCast);

	// Cast the argument value to the destination type
	CAST(argData, argType, dstType, &argCast);

	// Perform the multiplication, placing the result in the destination variable
	DO_OP(dstData, &srcCast, &argCast, dstType, *, FMUL, FMUL);

	return 1;
}

int vmm_div(env_t *env, byte_t **argLoc)
{
	const char *srcName = (const char *)argLoc;
	const char *argName;
	const char *dstName;
	data_t *srcData, *argData, *dstData;
	flags_t srcFlags, argFlags, dstFlags;
	byte_t srcType, argType, dstType;
	data_t srcCast, argCast;

	// Get the destination variable and its type
	TRY_FETCH_NEXT_VAR(env, argLoc, &dstName, &dstData, &dstType, &dstFlags);

	// Get the source variable and its type
	TRY_FETCH_NEXT_VAR(env, argLoc, &srcName, &srcData, &srcType, &srcFlags);

	// Get the type of the next argument in the div command and fetch the data stored
	// there, incrementing by the required number of bytes.
	TRY_FETCH_ARG_DATA(env, argLoc, &argType, &argData, &argFlags);

	// Cast the source value to the destination type
	CAST(srcData, srcType, dstType, &srcCast);

	// Cast the argument value to the destination type
	CAST(argData, argType, dstType, &argCast);

	// Perform the division, placing the result in the destination variable
	DO_OP(dstData, &srcCast, &argCast, dstType, /, FDIV, FDIV);

	return 1;
}

int vmm_mod(env_t *env, byte_t **argLoc)
{
	const char *srcName = (const char *)argLoc;
	const char *argName;
	const char *dstName;
	data_t *srcData, *argData, *dstData;
	flags_t srcFlags, argFlags, dstFlags;
	byte_t srcType, argType, dstType;
	data_t srcCast, argCast;

	// Get the destination variable and its type
	TRY_FETCH_NEXT_VAR(env, argLoc, &dstName, &dstData, &dstType, &dstFlags);

	// Get the source variable and its type
	TRY_FETCH_NEXT_VAR(env, argLoc, &srcName, &srcData, &srcType, &srcFlags);

	// Get the type of the next argument in the mod command and fetch the data stored
	// there, incrementing by the required number of bytes.
	TRY_FETCH_ARG_DATA(env, argLoc, &argType, &argData, &argFlags);

	// Cast the source value to the destination type
	CAST(srcData, srcType, dstType, &srcCast);

	// Cast the argument value to the destination type
	CAST(argData, argType, dstType, &argCast);

	// Perform the modulus operation, placing the result in the destination variable
	DO_OP(dstData, &srcCast, &argCast, dstType, %, fmodf, fmod);

	return 1;
}