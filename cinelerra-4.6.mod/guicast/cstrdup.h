#ifndef __CSTRDUP_H__
#define __CSTRDUP_H__

#include <stdarg.h>
#include <string.h>

static inline char *cstrcat(int n, ...) {
  int len = 0;  va_list va;  va_start(va,n);
  for(int i=0; i<n; ++i) len += strlen(va_arg(va,char*));
  va_end(va);  char *cp = new char[len+1], *bp = cp;  va_start(va,n);
  for(int i=0; i<n; ++i) for(char*ap=va_arg(va,char*); *ap; *bp++=*ap++); 
  va_end(va);  *bp = 0;
  return cp;
}
static inline char *cstrdup(const char *cp) {
  return strcpy(new char[strlen(cp)+1],cp);
}

#ifndef lengthof
#define lengthof(ary) ((int)(sizeof(ary)/sizeof(ary[0])))
#endif

#endif
