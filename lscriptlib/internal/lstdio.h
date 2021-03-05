#if !defined(LSTDIO_H)
#define LSTDIO_H

#include "../lscript.h"

#if defined(__cplusplus)
extern "C"
{
#endif

	LNIFUNC lulong LNICALL StdFileHandle_fopen(LEnv, lclass, lobject, lint);

	LNIFUNC void LNICALL StdFileHandle_fclose(LEnv, lclass, lulong);

	LNIFUNC void LNICALL StdFileHandle_fputc(LEnv, lclass, lulong, lchar);

	LNIFUNC luint LNICALL StdFileHandle_fwrite(LEnv, lclass, lulong, lchararray, luint, luint);

	LNIFUNC luint LNICALL StdFileHandle_fread(LEnv, lclass, lulong, lchararray, luint, luint);

	LNIFUNC luint LNICALL StdFileHandle_freadline(LEnv, lclass, lulong);

#if defined(__cplusplus)
}
#endif

#endif