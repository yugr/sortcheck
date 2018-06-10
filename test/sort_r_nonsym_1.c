/*
 * Copyright 2018 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#define _GNU_SOURCE
#include <stdlib.h>

char aa[] = { 1, 2, 3 };

// CHECK: comparison function is not symmetric
int cmp(const void *pa, const void *pb, void *a) {
  int *arg = (int *)a;
  return (*arg++ % 3) - 1;
}

int main() {
  int x = 0;
  qsort_r(aa, sizeof(aa), 1, cmp, &x);
  return 0;
}

