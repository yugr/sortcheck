#include <stdlib.h>

char aa[] = { 1, 2, 3 };

// CHECK: comparison function is not transitive
int cmp(const void *pa, const void *pb) {
  char a = *(const char *)pa;
  char b = *(const char *)pb;
  if(a == b)
    return 0;
  if(a == 1 && b == 2 || a == 2 && b == 3)
    return -1;
  if(a == 2 && b == 1 || a == 3 && b == 2)
    return 1;
  return 0;
}

int main() {
  qsort(aa, sizeof(aa), 1, cmp);
  return 0;
}

