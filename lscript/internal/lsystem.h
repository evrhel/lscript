#if !defined(LSYSTEM_H)
#define LSYSTEM_H

#include "../lscript.h"

LNIFUNC void LNICALL System_exit(LEnv, lclass, lint);

LNIFUNC void LNICALL System_loadLibrary(LEnv, lclass, lobject);

LNIFUNC lobject LNICALL System_getProperty(LEnv, lclass, lobject);

LNIFUNC lobject LNICALL System_setProperty(LEnv, lclass, lobject, lobject);

#endif