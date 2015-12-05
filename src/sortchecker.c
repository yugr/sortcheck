#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#include <dlfcn.h>
#include <syslog.h>

#define EXPORT __attribute__((visibility("default")))

static int debug = 0;
static int max_errors = 10;
static int print_to_syslog = 0;

static int init_done = 0;
static void __attribute__((constructor)) init() {
  const char *opts;
  if(!(opts = getenv("SORTCHECK_OPTIONS")))
    return;

  char *opts_ = strdup(opts);

  char *cur;
  for(cur = opts_; *cur; ) {
    // SORTCHECK_OPTIONS is a colon-separated list of assignments

    const char *name = cur;

    char *assign = strchr(cur, '=');
    if(!assign)
      break;
    *assign = 0;

    char *value = assign + 1;

    char *end = strchr(value, ':');
    if(end)
      *end = 0;
    cur = end ? end + 1 : value + strlen(value);

    if(0 == strcmp(name, "debug")) {
      debug = atoi(value);
    } else if(0 == strcmp(name, "print_to_syslog")) {
      print_to_syslog = atoi(value);;
    } else if(0 == strcmp(name, "max_errors")) {
      max_errors = atoi(value);
    } else {
      fprintf(stderr, "sortcheck: unknown option '%s'\n", name);
      exit(1);
    }
  }
  free(opts_);

  init_done = 1;
}

static int num_errors = 0;

typedef int (*cmp_fun_t)(const void *, const void *);

static unsigned checksum(const void *data, size_t sz) {
  // Fletcher's checksum
  uint16_t s1 = 0, s2 = 0;
  size_t i;
  for(i = 0; i < sz; ++i) {
    s1 = (s1 + ((const uint8_t *)data)[i]) % UINT8_MAX;
    s2 = (s2 + s1) % UINT8_MAX;
  }
  return s1 | (s2 << 8);
}

// TODO: print program name

#define report_error(where, fmt, ...) do {                                              \
  if(num_errors < max_errors) {                                                         \
    if(print_to_syslog)                                                                 \
      syslog(LOG_WARNING, "error in %s: " fmt "\n", where, ##__VA_ARGS__);              \
    else                                                                                \
      fprintf(stderr, "sortchecker: error in %s: " fmt "\n", where, ##__VA_ARGS__);     \
    ++num_errors;                                                                       \
  }                                                                                     \
} while(0)

// Check that comparator is stable and does not modify arguments
static void check_basic(const char *caller, cmp_fun_t cmp, const char *key, const void *data, size_t n, size_t sz) {
  const char *some = key ? key : data;

  size_t i;
  for(i = 0; i < n; ++i) {
    const void *elt = (const char *)data + i * sz;
    unsigned cs = checksum(elt, sz);

    int ord1, ord2;
    ord1 = cmp(some, elt);
    if(cs != checksum(elt, sz)) {
      report_error(caller, "comparison function modifies data");
      return;
    }

    ord2 = cmp(some, elt);
    if(ord1 != ord2) {
      report_error(caller, "comparison function returns unstable results");
      return;
    }

    ord1 = cmp(elt, elt);
    if(cs != checksum(elt, sz)) {
      report_error(caller, "comparison function modifies data");
      return;
    }

    if(ord1 != 0) {
      report_error(caller, "comparison function returns non-zero for equal elements");
      return;
    }

    ord2 = cmp(elt, elt);
    if(ord1 != ord2) {
      report_error(caller, "comparison function returns unstable results");
      return;
    }
  }
}

// Check that array is sorted
static void check_sorted(const char *caller, cmp_fun_t cmp, const char *key, const void *data, size_t n, size_t sz) {
  size_t i;

  if(key) {
    int order = 1;
    for(i = 0; i < n; ++i) {
      const void *elt = (const char *)data + i * sz;
      int new_order = cmp(key, elt);
      if(new_order > order) {
        report_error(caller, "processed array is not sorted");
	return;
      }
      order = new_order;
    }
  }

  for(i = 1; i < n; ++i) {
    const void *elt = (const char *)data + i * sz;
    const void *prev = (const char *)elt - sz;
    if(cmp(prev, elt) > 0) {
      report_error(caller, "processed array is not sorted");
      return;
    }
  }
}

// Check that ordering is total
static void check_total(const char *caller, cmp_fun_t cmp, const char *key, const void *data, size_t n, size_t sz) {
  key = key;  // TODO: check key as well

  int8_t cmp_[32][32];
  n = n > 32 ? 32 : n;
  memset(cmp_, 0, sizeof(cmp_));

  size_t i, j, k;
  for(i = 0; i < n; ++i)
  for(j = 0; j < n; ++j) {
    const void *a = (const char *)data + i * sz;
    const void *b = (const char *)data + j * sz;
    cmp_[i][j] = cmp(a, b);
  }

  // Following axioms from http://mathworld.wolfram.com/StrictOrder.html

  // Totality by construction

  // Irreflexivity + Asymmetricity
  for(i = 0; i < n; ++i)
  for(j = 0; j < n; ++j) {
    if(cmp_[i][j] != -cmp_[j][i]) {
      report_error(caller, "comparison function is not symmetric");
      return;
    }
  }

  // Transitivity
  // FIXME: slow slow...
  for(i = 0; i < n; ++i)
  for(j = 0; j < n; ++j)
  for(k = 0; k < n; ++k) {
    if(cmp_[i][j] == cmp_[j][k] && cmp_[i][j] != cmp_[i][k]) {
      report_error(caller, "comparison function is not transitive");
      return;
    }
  }
}

#define GET_REAL \
  static void *_real; \
  if(!_real) { \
    _real = dlsym(RTLD_NEXT, __func__); \
    if(debug) \
      fprintf(stderr, "Hello from %s interceptor!\n", __func__); \
  }

EXPORT void *bsearch(const void *key, const void *data, size_t n, size_t sz, cmp_fun_t cmp) {
  GET_REAL;
  check_basic(__func__, cmp, key, data, n, sz);
  check_total(__func__, cmp, 0, data, n, sz);  // manpage does not require this but still
  check_sorted(__func__, cmp, key, data, n, sz);
  return ((void *(*)(const void *, const void *, size_t, size_t, cmp_fun_t))_real)(key, data, n, sz, cmp);
}

EXPORT void qsort(void *data, size_t n, size_t sz, int (*cmp)(const void *, const void *)) {
  GET_REAL;
  check_basic(__func__, cmp, 0, data, n, sz);
  check_total(__func__, cmp, 0, data, n, sz);
  ((void *(*)(void *, size_t, size_t, cmp_fun_t))_real)(data, n, sz, cmp);
}

// TODO: qsort_r

