/*
 * Copyright 2015-2016 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#include <stdlib.h>

char aa[] = { 1 };

// CHECK-NOT: comparison function is not reflexive
int cmp(const void *pa, const void *pb) {
  return 1;
}

int main() {
  qsort(aa, sizeof(aa), 1, cmp);
  return 0;
}

