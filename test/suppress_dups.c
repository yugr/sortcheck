/*
 * Copyright 2018 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#include <stdlib.h>
#include <string.h>

char aa[] = { 1, 2, 3 };
int call_number;

// CMDLINE: a b c
// CHECK: comparison function returns unstable results
// CHECK-NOT: comparison function modifies data
int cmp(const void *pa, const void *pb) {
  static int x;
  switch(call_number) {
  case 0:
  default:
    return x++ % 2;
  case 1:
    memset(aa, 0, sizeof(aa));
    return (pa > pb) - (pa < pb);
  }
}

int main() {
  qsort(aa, sizeof(aa), 1, cmp);
  ++call_number;
  qsort(aa, sizeof(aa), 1, cmp);
  return 0;
}
