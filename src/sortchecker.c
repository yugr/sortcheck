#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>

#include <dlfcn.h>
#include <syslog.h>
#include <sys/types.h>
#include <unistd.h>

#include <checksum.h>
#include <proc_info.h>

#define EXPORT __attribute__((visibility("default")))

// Runtime options
static int debug = 0;
static int max_errors = 10;
static int print_to_syslog = 0;

// Other pieces of state
static int init_done = 0;
static ProcMap *maps = 0;
static char *proc_name = 0;
static char *proc_cmdline = 0;
static size_t nmaps = 0;
static int num_errors = 0;
static long proc_pid = -1;


static void fini() {
  if(maps)
    free(maps);
  if(proc_cmdline)
    free(proc_cmdline);
  if(proc_name)
    free(proc_name);
}

static void init() {
  const char *opts;
  if((opts = getenv("SORTCHECK_OPTIONS"))) {
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
	openlog("", 0, LOG_USER);
      } else if(0 == strcmp(name, "max_errors")) {
        max_errors = atoi(value);
      } else {
        fprintf(stderr, "sortcheck: unknown option '%s'\n", name);
        exit(1);
      }
    }
    free(opts_);
  }

  // Get mappings
  maps = get_proc_maps(&nmaps);
  if(debug) {
    fputs("Process map:\n", stderr);
    size_t i;
    for(i = 0; i < nmaps; ++i)
      fprintf(stderr, "  %50s: %p-%p\n", &maps[i].name[0], maps[i].begin_addr, maps[i].end_addr);
  }

  get_proc_cmdline(&proc_name, &proc_cmdline);

  proc_pid = (long)getpid();

  atexit(fini);

  init_done = 1;
}

typedef struct {
  const char *func;
  const void *ret_addr;
  const char *caller_module;
  size_t caller_offset;
} ErrorContext;

static void report_error(ErrorContext *ctx, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  if(num_errors >= max_errors)
    return;
  ++num_errors;

  if(!ctx->caller_module) {
    // Lazily compute caller's module (no race!)
    size_t i;
    for(i = 0; i < nmaps; ++i) {
      if(maps[i].begin_addr <= ctx->ret_addr && ctx->ret_addr < maps[i].end_addr) {
	ctx->caller_module = &maps[i].name[0];
	ctx->caller_offset = (const char *)ctx->ret_addr - (const char *)maps[i].begin_addr;
	break;
      }
    }
    if(i == nmaps) {
      ctx->caller_module = "<unknown>";
      ctx->caller_offset = 0;
    }
  }

  char body[128];
  vsnprintf(body, sizeof(body), fmt, ap);

  // TODO: some parts of the message may be precomputed
  char full_msg[256];
  snprintf(full_msg, sizeof(full_msg), "%s[%ld]: %s: %s (called from %p (%s+%zx), cmdline is \"%s\")\n", proc_name, proc_pid, ctx->func, body, ctx->ret_addr, ctx->caller_module, ctx->caller_offset, proc_cmdline);

  if(print_to_syslog)
    syslog(LOG_WARNING, "%s", full_msg);
  else
    fputs(full_msg, stderr);
}

typedef int (*cmp_fun_t)(const void *, const void *);

static inline int sign(int x) {
  return x < 0 ? -1 : x > 0 ? 1 : 0;
}

// Check that comparator is stable and does not modify arguments
static void check_basic(ErrorContext *ctx, cmp_fun_t cmp, const char *key, const void *data, size_t n, size_t sz) {
  const char *some = key ? key : data;
  size_t i;

  // Check for modifying comparison functions
  for(i = 0; i < n; ++i) {
    const void *elt = (const char *)data + i * sz;
    unsigned cs = checksum(elt, sz);
    cmp(some, elt);
    if(cs != checksum(elt, sz)) {
      report_error(ctx, "comparison function modifies data");
      break;
    }
    cmp(elt, elt);
    if(cs != checksum(elt, sz)) {
      report_error(ctx, "comparison function modifies data");
      break;
    }
  }

  // Check for non-constant return value
  for(i = 0; i < n; ++i) {
    const void *elt = (const char *)data + i * sz;
    if(cmp(some, elt) != cmp(some, elt)) {
      report_error(ctx, "comparison function returns unstable results");
      break;
    }
  }

  // Check that equality is respected
  for(i = 0; i < n; ++i) {
    const void *elt = (const char *)data + i * sz;
    // TODO: it may make sense to compare different but equal elements?
    if(0 != cmp(elt, elt)) {
      report_error(ctx, "comparison function returns non-zero for equal elements");
      break;
    }
  }
}

// Check that array is sorted
static void check_sorted(ErrorContext *ctx, cmp_fun_t cmp, const char *key, const void *data, size_t n, size_t sz) {
  size_t i;

  if(key) {
    int order = 1;
    for(i = 0; i < n; ++i) {
      const void *elt = (const char *)data + i * sz;
      int new_order = sign(cmp(key, elt));
      if(new_order > order) {
        report_error(ctx, "processed array is not sorted at index %zd", i);
	return;  // Return to stop further error reporting
      }
      order = new_order;
    }
  }

  for(i = 1; i < n; ++i) {
    const void *elt = (const char *)data + i * sz;
    const void *prev = (const char *)elt - sz;
    if(cmp(prev, elt) > 0) {
      report_error(ctx, "processed array is not sorted at index %zd", i);
      break;
    }
  }
}

// Check that ordering is total
static void check_total(ErrorContext *ctx, cmp_fun_t cmp, const char *key, const void *data, size_t n, size_t sz) {
  key = key;  // TODO: check key as well

  int8_t cmp_[32][32];
  n = n > 32 ? 32 : n;
  memset(cmp_, 0, sizeof(cmp_));

  size_t i, j, k;
  for(i = 0; i < n; ++i)
  for(j = 0; j < n; ++j) {
    const void *a = (const char *)data + i * sz;
    const void *b = (const char *)data + j * sz;
    cmp_[i][j] = sign(cmp(a, b));
  }

  // Following axioms from http://mathworld.wolfram.com/StrictOrder.html

  // Totality by construction

  // Irreflexivity + Asymmetricity
  for(i = 0; i < n; ++i)
  for(j = 0; j < n; ++j) {
    if(cmp_[i][j] != -cmp_[j][i]) {
      report_error(ctx, "comparison function is not symmetric");
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
      report_error(ctx, "comparison function is not transitive");
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
  ErrorContext ctx = { __func__, __builtin_return_address(0), 0, 0 };
  check_basic(&ctx, cmp, key, data, n, sz);
  check_total(&ctx, cmp, 0, data, n, sz);  // manpage does not require this but still
  check_sorted(&ctx, cmp, key, data, n, sz);
  return _real(key, data, n, sz, cmp);
}

EXPORT void qsort(void *data, size_t n, size_t sz, int (*cmp)(const void *, const void *)) {
  MAYBE_INIT;
  GET_REAL(qsort);
  ErrorContext ctx = { __func__, __builtin_return_address(0), 0, 0 };
  check_basic(&ctx, cmp, 0, data, n, sz);
  check_total(&ctx, cmp, 0, data, n, sz);
  _real(data, n, sz, cmp);
}

// TODO: qsort_r

