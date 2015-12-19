// OPTS: max_errors=1

#include <stdlib.h>

char aa[] = { 1, 2, 3 };

int cmp(const void *pa, const void *pb) {
  char a = *(const char *)pa;
  char b = *(const char *)pb;
  // CHECK: comparison function modifies data
  int res = a == b ? 1 : 0;
  // CHECK-NOT: comparison function is not symmetric
  *(char *)pa = 100;
  return res;
}

int main() {
  qsort(aa, sizeof(aa), 1, cmp);
  int key = 1;
  bsearch(&key, aa, sizeof(aa), 1, cmp);
  return 0;
}

