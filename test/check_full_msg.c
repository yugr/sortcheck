#include <stdlib.h>

char aa[] = { 1, 2, 3 };

// CMDLINE: a b c
// CHECK: a.out.*: qsort: comparison function returns unstable results (called from .*a.out+.*, cmdline is ".*/a.out a b c")
int cmp(const void *pa, const void *pb) {
  static int x;
  return x++ % 2;
}

int main() {
  qsort(aa, sizeof(aa), 1, cmp);
  return 0;
}

