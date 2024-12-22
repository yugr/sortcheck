/*
 * Copyright 2015-2016 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#include <stdlib.h>

char aa[] = { 1, 2, 3 };

// OPTS: check=no_all,basic:print_to_syslog=1
// REQUIRE: syslog
// SYSLOG: comparison function modifies data
int cmp(const void *pa, const void *pb) {
  char a = *(const char *)pa;
  char b = *(const char *)pb;
  int res = a < b ? -1 : a == b ? 0 : 1;
  *(char *)pa = *(char *)pa + 1;
  return res;
}

int main() {
  qsort(aa, sizeof(aa), 1, cmp);
  return 0;
}

