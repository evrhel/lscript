#if !defined(LMATH_H)
#define LMATH_H

#include "../lscript.h"

#if defined(__cplusplus)
extern "C"
{
#endif

	LNIFUNC ldouble LNICALL lscript_lang_(LEnv venv, lclass vclazz, ldouble a);
	LNIFUNC lfloat LNICALL lscript_lang_Math_roundf(LEnv venv, lclass vclazz, lfloat a);

	LNIFUNC ldouble LNICALL lscript_lang_Math_floor(LEnv venv, lclass vclazz, ldouble a);
	LNIFUNC lfloat LNICALL lscript_lang_Math_floorf(LEnv venv, lclass vclazz, lfloat a);

	LNIFUNC ldouble LNICALL lscript_lang_Math_ceil(LEnv venv, lclass vclazz, ldouble a);
	LNIFUNC lfloat LNICALL lscript_lang_Math_ceilf(LEnv venv, lclass vclazz, lfloat a);

	LNIFUNC ldouble LNICALL lscript_lang_Math_sqrt(LEnv venv, lclass vclazz, ldouble a);
	LNIFUNC lfloat LNICALL lscript_lang_Math_sqrtf(LEnv venv, lclass vclazz, lfloat a);

	LNIFUNC ldouble LNICALL lscript_lang_Math_cbrt(LEnv venv, lclass vclazz, ldouble a);
	LNIFUNC lfloat LNICALL lscript_lang_Math_cbrtf(LEnv venv, lclass vclazz, lfloat a);

	LNIFUNC ldouble LNICALL lscript_lang_Math_exp(LEnv venv, lclass vclazz, ldouble a);
	LNIFUNC lfloat LNICALL lscript_lang_Math_expf(LEnv venv, lclass vclazz, lfloat a);

	LNIFUNC ldouble LNICALL lscript_lang_Math_exp2(LEnv venv, lclass vclazz, ldouble a);
	LNIFUNC lfloat LNICALL lscript_lang_Math_exp2f(LEnv venv, lclass vclazz, lfloat a);

	LNIFUNC ldouble LNICALL lscript_lang_Math_log(LEnv venv, lclass vclazz, ldouble a);
	LNIFUNC lfloat LNICALL lscript_lang_Math_logf(LEnv venv, lclass vclazz, lfloat a);

	LNIFUNC ldouble LNICALL lscript_lang_Math_log10(LEnv venv, lclass vclazz, ldouble a);
	LNIFUNC lfloat LNICALL lscript_lang_Math_log10f(LEnv venv, lclass vclazz, lfloat a);

	LNIFUNC ldouble LNICALL lscript_lang_Math_sin(LEnv venv, lclass vclazz, ldouble a);
	LNIFUNC lfloat LNICALL lscript_lang_Math_sinf(LEnv venc, lclass vclazz, lfloat a);

	LNIFUNC ldouble LNICALL lscript_lang_Math_cos(LEnv venv, lclass vclazz, ldouble a);
	LNIFUNC lfloat LNICALL lscript_lang_Math_cosf(LEnv venv, lclass vclazz, lfloat a);

	LNIFUNC ldouble LNICALL lscript_lang_Math_tan(LEnv venv, lclass vclazz, ldouble a);
	LNIFUNC lfloat LNICALL lscript_lang_Math_tanf(LEnv venv, lclass vclazz, lfloat a);

#if defined(__cplusplus)
}
#endif

#endif