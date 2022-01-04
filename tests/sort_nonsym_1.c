/*
 * Copyright 2015-2016 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#include <stdlib.h>

char aa[] = { 1, 2, 3 };

// CHECK: comparison function is not symmetric
int cmp(const void *pa, const void *pb) {
  char a = *(const char *)pa;
  char b = *(const char *)pb;
  return a < b ? -1 : a == b ? 0 : -1;
}

int main() {
  qsort(aa, sizeof(aa), 1, cmp);
  return 0;
}

