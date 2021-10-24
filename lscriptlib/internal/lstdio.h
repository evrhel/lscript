#if !defined(LSTDIO_H)
#define LSTDIO_H

#include "../lscript.h"

#if defined(__cplusplus)
extern "C"
{
#endif

	LNIFUNC lulong LNICALL lscript_io_StdFileHandle_fopen(LEnv venv, lclass vclazz, lobject filepath, lint mode);

	LNIFUNC void LNICALL lscript_io_StdFileHandle_fclose(LEnv venv, lclass vclazz, lulong handle);

	LNIFUNC void LNICALL lscript_io_StdFileHandle_fputc(LEnv venv, lclass vclazz, lulong handle, lchar c);

	LNIFUNC luint LNICALL lscript_io_StdFileHandle_fwrite(LEnv venv, lclass vclazz, lulong handle, lchararray data, luint off, luint length);

	LNIFUNC luint LNICALL lscript_io_StdFileHandle_fread(LEnv venv, lclass vclazz, lulong handle, lchararray buf, luint off, luint count);

	LNIFUNC lchararray LNICALL lscript_io_StdFileHandle_freadline(LEnv venv, lclass vclazz, lulong handle);

#if defined(__cplusplus)
}
#endif

#endif