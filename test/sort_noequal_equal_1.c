#include <stdlib.h>

char aa[] = { 1 };

// CHECK-NOT: comparison function is not reflexive
int cmp(const void *pa, const void *pb) {
  char a = *(const char *)pa;
  char b = *(const char *)pb;
  return 1;
}

int main() {
  qsort(aa, sizeof(aa), 1, cmp);
  return 0;
}

