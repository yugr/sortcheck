/*
 * Copyright 2015-2022 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#ifndef FLAGS_H
#define FLAGS_H

enum CheckFlags {
  CHECK_BASIC        = 1 << 0,
  CHECK_REFLEXIVITY  = 1 << 1,
  CHECK_SYMMETRY     = 1 << 2,
  CHECK_TRANSITIVITY = 1 << 3,
  CHECK_SORTED       = 1 << 4,
  CHECK_GOOD_BSEARCH = 1 << 5,
  CHECK_UNIQUE       = 1 << 6,
  // Do not check reflexivity because it's normally not important (CHECK_REFLEXIVITY).
  // Do not assume bsearch is commutative (CHECK_GOOD_BSEARCH).
  CHECK_DEFAULT      = CHECK_BASIC | CHECK_SYMMETRY
                       | CHECK_TRANSITIVITY | CHECK_SORTED,
  CHECK_ALL          = 0xffffffff,
};

typedef struct {
  unsigned char debug : 1;
  unsigned char report_error : 1;
  unsigned char print_to_syslog : 1;
  unsigned char raise : 1;
  unsigned max_errors;
  unsigned sleep;
  unsigned checks;
  int start;
  unsigned extent;
  const char *out_filename;
} Flags;

int parse_flags(char *opts, Flags *flags);

#endif
