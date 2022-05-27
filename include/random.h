/*
 * Copyright 2022 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#ifndef RAND_H
#define RAND_H

void random_reseed(unsigned long seed);

unsigned long random_seed();

unsigned long random_generate();

#endif
