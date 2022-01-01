/*
 * Copyright 2015-2018 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#ifndef IO_H
#define IO_H

#include <stddef.h> // size_t

char *read_file(const char *fname, size_t *plen);

#endif
