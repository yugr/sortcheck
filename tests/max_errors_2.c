/*
 * Copyright 2015-2024 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

// OPTS: max_errors=1

#include <stdlib.h>

char aa[] = { 1, 2, 3 };

// Asan preruns comparator on array which ruins logic below
// SKIPPED: asan

int cmp(const void *pa, const void *pb) {
  char a = *(const char *)pa;
  char b = *(const char *)pb;
  // CHECK-NOT: comparison function is not symmetric
  int res = a == b ? 1 : 0;
  // CHECK: comparison function modifies data
  *(char *)pa = *(char *)pa + 1;
  return res;
}

int main() {
  qsort(aa, sizeof(aa), 1, cmp);
  int key = 1;
  bsearch(&key, aa, sizeof(aa), 1, cmp);
  return 0;
}

