/*
 * Copyright 2015-2016 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stddef.h>  // size_t

// Fletcher's checksum
unsigned checksum(const void *data, size_t sz);

#endif
