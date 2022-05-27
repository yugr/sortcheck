/*
 * Copyright 2022 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#ifndef RANDOM_H
#define RANDOM_H

static unsigned long init;
static unsigned long next;

void random_reseed(unsigned long s) {
  init = next = s;
}

unsigned long random_seed() {
  return init;
}

unsigned random_generate() {
  next = next * 1103515245 + 12345;
        return (unsigned int)(next/65536) % 32768;

}

#endif
