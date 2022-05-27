/*
 * Copyright 2015-2022 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#include <flags.h>
#include <random.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>

int parse_flags(char *opt, Flags *flags) {
  struct timeval tv;
  if (0 != gettimeofday(&tv, 0)) {
    fprintf(stderr, "sortcheck: gettimeofday failed\n");
    return 0;
  }
  random_reseed((unsigned long)tv.tv_usec);

  // Skip parasite newlines inserted by some editors
  size_t newline = strcspn(opt, "\r\n");
  opt[newline] = 0;

  for(; *opt; ) {
    // Flags is a colon-separated list of assignments

    const char *name = opt;

    char *assign = strchr(opt, '=');
    if(!assign) {
      fprintf(stderr, "sortcheck: missing '=' in '%s'\n", opt);
      return 0;
    }
    *assign = 0;

    char *value = assign + 1;

    char *end = strchr(value, ':');
    if(end)
      *end = 0;
    opt = end ? end + 1 : value + strlen(value);

    if(0 == strcmp(name, "debug")) {
      flags->debug = atoi(value);
    } else if(0 == strcmp(name, "print_to_syslog")) {
      flags->print_to_syslog = atoi(value);
    } else if(0 == strcmp(name, "print_to_file")) {
      flags->out_filename = strdup(value);
    } else if(0 == strcmp(name, "report_error")) {
      flags->report_error = atoi(value);
    } else if(0 == strcmp(name, "max_errors")) {
      int max_errors = atoi(value);
      if (max_errors >= 0)
        flags->max_errors = max_errors < 1024 ? max_errors : 1024;;
    } else if(0 == strcmp(name, "raise")) {
      flags->raise = atoi(value);
    } else if(0 == strcmp(name, "sleep")) {
      flags->sleep = atoi(value);
    } else if(0 == strcmp(name, "start")) {
      flags->start = atoi(value);
    } else if(0 == strcmp(name, "extent")) {
      flags->extent = atoi(value);
    } else if(0 == strcmp(name, "check")) {
      unsigned checks = 0;
      do {
        char *next = strchr(value, ',');
        if(next) {
          *next = 0;
          ++next;
        }

        int no = 0;
        if(0 == strncmp(value, "no_", 3)) {
          no = 1;
          value += 3;
        }

#define PARSE_CHECK(m, s) if(0 == strcmp(s, value)) { \
  if(no) checks &= ~m; else checks |= m; \
} else
        PARSE_CHECK(CHECK_BASIC, "basic")
        PARSE_CHECK(CHECK_REFLEXIVITY, "reflexivity")
        PARSE_CHECK(CHECK_SYMMETRY, "symmetry")
        PARSE_CHECK(CHECK_TRANSITIVITY, "transitivity")
        PARSE_CHECK(CHECK_SORTED, "sorted")
        PARSE_CHECK(CHECK_GOOD_BSEARCH, "good_bsearch")
        PARSE_CHECK(CHECK_UNIQUE, "unique")
        PARSE_CHECK(CHECK_DEFAULT, "default")
        PARSE_CHECK(CHECK_ALL, "all")
        {
          fprintf(stderr, "sortcheck: unknown check '%s'\n", value);
          return 0;
        }
        value = next;
      } while(value);
      flags->checks = checks;
    } else if(0 == strcmp(name, "seed")) {
      random_reseed((unsigned long)atoi(value));
    } else {
      fprintf(stderr, "sortcheck: unknown option '%s'\n", name);
      return 0;
    }
  }
  return 1;
}

