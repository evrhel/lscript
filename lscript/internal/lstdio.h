#if !defined(LSTDIO_H)
#define LSTDIO_H

#include "../lscript.h"

LNIFUNC lulong LNICALL ls_open(LEnv, lclass, lobject, lint);

LNIFUNC void LNICALL ls_close(LEnv, lclass, lulong);

LNIFUNC void LNICALL ls_putc(LEnv, lclass, lulong, lchar);

LNIFUNC luint LNICALL ls_write(LEnv, lclass, lulong, lchararray, luint, luint);

LNIFUNC void LNICALL putls(LEnv, lclass, lobject);

LNIFUNC void LNICALL test_call(LEnv, lclass, lint, lint, lint, lint);

#endif