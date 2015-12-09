#include <stdio.h>

#include <io.h>

char *read_file(const char *fname, size_t *plen) {
  *plen = 0;

  FILE *p = fopen(fname, "rb");
  if(!p)
    return 0;

  size_t bufsize = 128, len = 0;
  char *res = malloc(bufsize);
  while(1) {
    size_t maxread = bufsize - len;
    size_t nread = fread(res + len, 1, maxread, p);
    len += nread;
    if(nread < maxread)
      break;
    bufsize *= 2;
    res = realloc(res, bufsize);
  }
  fclose(p);

  *plen = len;
  return res;
}
