/*
 * Copyright 2015-2016 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#include <checksum.h>

#include <stdint.h>

unsigned checksum(const void *data, size_t sz) {
  uint16_t s1 = 0, s2 = 0;
  size_t i;
  for(i = 0; data && i < sz; ++i) {
    s1 = (s1 + ((const uint8_t *)data)[i]) % UINT8_MAX;
    s2 = (s2 + s1) % UINT8_MAX;
  }
  return s1 | (s2 << 8);
}


