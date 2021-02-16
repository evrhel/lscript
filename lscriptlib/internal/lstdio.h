#if !defined(LSTDIO_H)
#define LSTDIO_H

#include "../lscript.h"

#if defined(__cplusplus)
extern "C"
{
#endif

	LNIFUNC lulong LNICALL FileOutputStream_fopen(LEnv, lclass, lobject, lint);

	LNIFUNC void LNICALL FileOutputStream_fclose(LEnv, lclass, lulong);

	LNIFUNC void LNICALL FileOutputStream_fputc(LEnv, lclass, lulong, lchar);

	LNIFUNC luint LNICALL FileOutputStream_fwrite(LEnv, lclass, lulong, lchararray, luint, luint);

#if defined(__cplusplus)
}
#endif

#endif