#ifndef M_MEMORY_H
#define M_MEMORY_H

#include <stdlib.h>
#include <string.h>

#define MALLOC malloc
#define FREE free
#define CALLOC calloc
#define REALLOC realloc
#define MEMSET memset
#define MEMZERO(A,S) memset((A),0,(S))

#ifndef min
#define min(x,y) ((x)>(y)?(y):(x))
#endif
#ifndef max
#define max(x,y) ((x)>(y)?(x):(y))
#endif
#ifndef MIN
#define MIN min
#endif
#ifndef MAX
#define MAX max
#endif

#endif
