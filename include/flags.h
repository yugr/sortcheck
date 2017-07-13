/*
 * Copyright 2015-2016 Yury Gribov
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
  CHECK_DEFAULT      = CHECK_BASIC | CHECK_SYMMETRY
                       | CHECK_TRANSITIVITY | CHECK_SORTED,
  CHECK_ALL          = 0xffffffff,
};

typedef struct {
  char debug;
  char max_errors;
  char report_error;
  char print_to_syslog;
  char raise;
  unsigned checks;
  const char *out_filename;
} Flags;

int parse_flags(char *opts, Flags *flags);

#endif
