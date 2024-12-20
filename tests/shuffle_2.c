/*
 * Copyright 2024 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#include <stdlib.h>

char aa[33] = {
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  100
};

// OPTS: shuffle=0
// CHECK: comparison function is not symmetric
int cmp(const void *pa, const void *pb) {
  char a = *(const char *)pa;
  return a == 100 ? 1 : 0;
}

int main() {
  qsort(aa, sizeof(aa), 1, cmp);
  return 0;
}
