/*
 * Copyright 2015-2016 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef __GNUC__
#define EXPORT __attribute__((visibility("default")))

// TODO: proper atomics here
#define barrier() asm("");

#else
#error "Unknown compiler"
#endif

#endif

