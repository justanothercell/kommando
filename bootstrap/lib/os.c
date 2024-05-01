#include "defines.c"

#ifdef __WIN32__
#include "direct.h"
#define chdir _chdir
#define mkdir _mkdir
#else
#include "<unistd.h>"
#endif