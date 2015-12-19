#include <stdlib.h>

char aa[] = { 1, 2, 3 };

// OPTS: check=no_all,basic
// CHECK: comparison function modifies data
int cmp(const void *pa, const void *pb) {
  char a = *(const char *)pa;
  char b = *(const char *)pb;
  int res = a < b ? -1 : a == b ? 0 : 1;
  *(char *)pa = *(char *)pb = 100;
  return res;
}

int main() {
  qsort(aa, sizeof(aa), 1, cmp);
  return 0;
}

