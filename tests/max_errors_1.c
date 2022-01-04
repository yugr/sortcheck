/*
 * Copyright 2015-2016 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#include <stdlib.h>

char aa[] = { 1, 2, 3 };

int cmp(const void *pa, const void *pb) {
  char a = *(const char *)pa;
  char b = *(const char *)pb;
  // CHECK: comparison function modifies data
  int res = a == b ? 1 : 0;
  // CHECK: comparison function is not symmetric
  *(char *)pa = 100;
  return res;
}

int main() {
  qsort(aa, sizeof(aa), 1, cmp);
  int key = 1;
  bsearch(&key, aa, sizeof(aa), 1, cmp);
  return 0;
}

