/*
 * Copyright 2022 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#include <stdlib.h>

// OPTS: start=8:extent=2
char aa[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

// CHECK: comparison function is not symmetric
int cmp(const void *pa, const void *pb) {
  const char *a = (const char *)pa;
  const char *b = (const char *)pb;
  // Normal path
  if (a < &aa[8] && b < &aa[8])
    return *a - *b;
  // Error path
  return 1;
}

int main() {
  qsort(aa, sizeof(aa), 1, cmp);
  return 0;
}

