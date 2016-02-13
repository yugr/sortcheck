/*
 * Copyright 2015-2016 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#include <stdlib.h>

char aa[] = { 1, 2, 3 };

// OPTS: report_error=0
// CHECK-NOT: qsort: .* (called from
int cmp(const void *pa, const void *pb) {
  static int x;
  return x++ % 2;
}

int main() {
  qsort(aa, sizeof(aa), 1, cmp);
  return 0;
}

