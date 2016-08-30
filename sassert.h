#ifndef SASSERT_H_INCLUDED
#define SASSERT_H_INCLUDED

#include <stdbool.h>
#include <stdlib.h>

#ifdef DEBUG

#   include <stdio.h>
#   define SASSERT_DEBUGPRINT(x)   fprintf(stderr, "Assertion failed at line %d of file %s: (%s)\r\n", __LINE__, __FILE__, x)

#else /* DEBUG */

#   define SASSERT_DEBUGPRINT(x)    {}

#endif /* DEBUG */

#define SASSERT(x)      do{ if (false == (x)){ SASSERT_DEBUGPRINT(#x); abort(); } }while(0)
#define SUNREACHABLE()  do{ SASSERT_DEBUGPRINT("Unreachable condition!"); abort(); }while(0)

#endif /* SASSERT_H_INCLUDED */
