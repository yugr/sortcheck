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
static void init() {
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

#define report_error(where, fmt, ...) do {                        \
  if(num_errors < max_errors) {                                   \
    if(print_to_syslog)                                           \
      syslog(LOG_WARNING, "%s: " fmt "\n", where, ##__VA_ARGS__); \
    else                                                          \
      fprintf(stderr, "%s: " fmt "\n", where, ##__VA_ARGS__);     \
    ++num_errors;                                                 \
  }                                                               \
} while(0)

// Check that comparator is stable and does not modify arguments
static void check_basic(const char *caller, cmp_fun_t cmp, const char *key, const void *data, size_t n, size_t sz) {
  const char *some = key ? key : data;
  size_t i;

  // Check for modifying comparison functions
  for(i = 0; i < n; ++i) {
    const void *elt = (const char *)data + i * sz;
    unsigned cs = checksum(elt, sz);
    cmp(some, elt);
    if(cs != checksum(elt, sz)) {
      report_error(caller, "comparison function modifies data");
      break;
    }
    cmp(elt, elt);
    if(cs != checksum(elt, sz)) {
      report_error(caller, "comparison function modifies data");
      break;
    }
  }

  // Check for non-constant return value
  for(i = 0; i < n; ++i) {
    const void *elt = (const char *)data + i * sz;
    if(cmp(some, elt) != cmp(some, elt)) {
      report_error(caller, "comparison function returns unstable results");
      break;
    }
  }

  // Check that equality is respected
  for(i = 0; i < n; ++i) {
    const void *elt = (const char *)data + i * sz;
    // TODO: it may make sense to compare different but equal elements?
    if(0 != cmp(elt, elt)) {
      report_error(caller, "comparison function returns non-zero for equal elements");
      break;
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
	return;  // Return to stop further error reporting
      }
      order = new_order;
    }
  }

  for(i = 1; i < n; ++i) {
    const void *elt = (const char *)data + i * sz;
    const void *prev = (const char *)elt - sz;
    if(cmp(prev, elt) > 0) {
      report_error(caller, "processed array is not sorted");
      break;
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
      goto sym_check_done;
    }
  }
sym_check_done:

  // Transitivity
  // FIXME: slow slow...
  for(i = 0; i < n; ++i)
  for(j = 0; j < n; ++j)
  for(k = 0; k < n; ++k) {
    if(cmp_[i][j] == cmp_[j][k] && cmp_[i][j] != cmp_[i][k]) {
      report_error(caller, "comparison function is not transitive");
      goto trans_check_done;
    }
  }
trans_check_done:
  return;
}

#define GET_REAL(sym)                                        \
  static typeof(sym) *_real;                                 \
  if(!_real) {                                               \
    _real = (typeof(sym) *)dlsym(RTLD_NEXT, #sym);           \
    if(debug)                                                \
      fprintf(stderr, "Hello from %s interceptor!\n", #sym); \
  }

#define MAYBE_INIT do {  \
  if(!init_done) init(); \
} while(0)

EXPORT void *bsearch(const void *key, const void *data, size_t n, size_t sz, cmp_fun_t cmp) {
  MAYBE_INIT;
  GET_REAL(bsearch);
  check_basic(__func__, cmp, key, data, n, sz);
  check_total(__func__, cmp, 0, data, n, sz);  // manpage does not require this but still
  check_sorted(__func__, cmp, key, data, n, sz);
  return _real(key, data, n, sz, cmp);
}

EXPORT void qsort(void *data, size_t n, size_t sz, int (*cmp)(const void *, const void *)) {
  MAYBE_INIT;
  GET_REAL(qsort);
  check_basic(__func__, cmp, 0, data, n, sz);
  check_total(__func__, cmp, 0, data, n, sz);
  _real(data, n, sz, cmp);
}

// TODO: qsort_r

