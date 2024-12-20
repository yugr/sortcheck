/*
 * Copyright 2015-2016 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#include <stdlib.h>

char k = 1;
char aa[] = { 1, 2, 3 };

// TODO: why this fails under Asan?
// SKIPPED: asan

// OPTS: check=default,good_bsearch
// CHECK: comparison function is not symmetric
int cmp(const void *pa, const void *pb) {
  char a = *(const char *)pa;
  char b = *(const char *)pb;
  return a < b ? -1 : a == b ? 0 : -1;
}

int main() {
  bsearch(&k, aa, sizeof(aa), 1, cmp);
  return 0;
}

