#ifndef LIB_OS_H
#define LIB_OS_H

#include "defines.h"

#ifdef __WIN32__
#include "direct.h"
#define chdir _chdir
#define mkdir _mkdir
#else
#include "<unistd.h>"
#endif

#endif