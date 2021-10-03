/*
 * Copyright 2021 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#include <stdlib.h>

char aa[128];

// CHECK-NOT: qsort: .* called from
int cmp(const void *pa, const void *pb) {
  return (const char *)pa < &aa[32] ? 0 : 1;
}

int main() {
  qsort(aa, sizeof(aa), 1, cmp);
  return 0;
}

