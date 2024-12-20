/*
 * Copyright 2015-2024 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#include <stdlib.h>

char aa[] = { 1, 2, 3 };

// Under Asan qsort and callback are intercepted
// so addresses are located in libasan (not a.out)
// SKIPPED: asan

// CMDLINE: a b c
// CHECK: a.out.*: qsort: comparison function returns unstable results (comparison function .*a.out+.*, called from .*a.out+.*, cmdline is ".*/a.out a b c")
int cmp(const void *pa, const void *pb) {
  static int x;
  return x++ % 2;
}

int main() {
  qsort(aa, sizeof(aa), 1, cmp);
  return 0;
}

