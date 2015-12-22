#include <checksum.h>
#include <proc_info.h>

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

#define EXPORT __attribute__((visibility("default")))

enum CheckFlags {
  CHECK_BASIC        = 1 << 0,
  CHECK_REFLEXIVITY  = 1 << 1,
  CHECK_SYMMETRY     = 1 << 2,
  CHECK_TRANSITIVITY = 1 << 3,
  CHECK_SORTED       = 1 << 4,
  CHECK_GOOD_BSEARCH = 1 << 5,
  CHECK_DEFAULT      = CHECK_BASIC | CHECK_SYMMETRY
                       | CHECK_TRANSITIVITY | CHECK_SORTED,
  CHECK_ALL          = 0xffffffff,
};

// Runtime options
static int debug = 0;
static int max_errors = 10;
static unsigned check_flags = CHECK_DEFAULT;
static FILE *out;

// Other pieces of state
static volatile int init_in_progress = 0, init_done = 0;
static ProcMap *maps = 0;
static char *proc_name = 0;
static char *proc_cmdline = 0;
static size_t nmaps = 0;
static int num_errors = 0;
static long proc_pid = -1;
static int do_report_error = 1;

static void fini(void) {
  if(maps)
    free(maps);
  if(proc_cmdline)
    free(proc_cmdline);
  if(proc_name)
    free(proc_name);
}

static void init(void) {
  if(init_done)
    return;

  // TODO: proper atomics here
  if(init_in_progress) {
    while(!init_done)
      asm("");
    return;
  }

  init_in_progress = 1;
  asm("");

  const char *out_filename = 0;
  int print_to_syslog = 0;

  const char *opts;
  char *opts_ = 0;
  if((opts = getenv("SORTCHECK_OPTIONS"))) {
    opts_ = strdup(opts);

    char *cur;
    for(cur = opts_; *cur; ) {
      // SORTCHECK_OPTIONS is a colon-separated list of assignments

      const char *name = cur;

      char *assign = strchr(cur, '=');
      if(!assign) {
        fprintf(stderr, "sortcheck: missing '=' in '%s'\n", cur);
        exit(1);
      }
      *assign = 0;

      char *value = assign + 1;

      char *end = strchr(value, ':');
      if(end)
        *end = 0;
      cur = end ? end + 1 : value + strlen(value);

      if(0 == strcmp(name, "debug")) {
        debug = atoi(value);
      } else if(0 == strcmp(name, "print_to_syslog")) {
        print_to_syslog = atoi(value);
      } else if(0 == strcmp(name, "print_to_file")) {
        out_filename = value;
      } else if(0 == strcmp(name, "report_error")) {
        do_report_error = atoi(value);
      } else if(0 == strcmp(name, "max_errors")) {
        max_errors = atoi(value);
      } else if(0 == strcmp(name, "check")) {
        unsigned flags = 0;
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
  if(no) flags &= ~m; else flags |= m; \
} else
          PARSE_CHECK(CHECK_BASIC, "basic")
          PARSE_CHECK(CHECK_REFLEXIVITY, "reflexivity")
          PARSE_CHECK(CHECK_SYMMETRY, "symmetry")
          PARSE_CHECK(CHECK_TRANSITIVITY, "transitivity")
          PARSE_CHECK(CHECK_SORTED, "sorted")
          PARSE_CHECK(CHECK_GOOD_BSEARCH, "good_bsearch")
          PARSE_CHECK(CHECK_DEFAULT, "default")
          PARSE_CHECK(CHECK_ALL, "all")
          {
            fprintf(stderr, "sortcheck: unknown check '%s'\n", value);
            exit(1);
          }
          value = next;
        } while(value);
        check_flags = flags;
      } else {
        fprintf(stderr, "sortcheck: unknown option '%s'\n", name);
        exit(1);
      }
    }
  }

  if(print_to_syslog && out_filename) {
    fprintf(stderr, "sortcheck: both print_to_syslog and print_to_file were specified\n");
    exit(1);
  } else if(print_to_syslog) {
    openlog("", 0, LOG_USER);
  } else if(out_filename) {
    if(!(out = fopen(out_filename, "ab"))) {
      fprintf(stderr, "sortcheck: failed to open %s for writing\n", out_filename);
      exit(1);
    }
  } else
    out = stderr;

  // Get mappings
  maps = get_proc_maps(&nmaps);
  if(debug) {
    fputs("Process map:\n", out);
    size_t i;
    for(i = 0; i < nmaps; ++i)
      fprintf(out, "  %50s: %p-%p\n", &maps[i].name[0], maps[i].begin_addr, maps[i].end_addr);
  }

  get_proc_cmdline(&proc_name, &proc_cmdline);

  proc_pid = (long)getpid();

  atexit(fini);

  if(opts_)
    free(opts_);

  // TODO: proper atomics here
  asm("");
  init_done = 1;
  init_in_progress = 0;
}

typedef struct {
  const char *func;
  const void *cmp_addr;
  const char *cmp_module;
  size_t cmp_offset;
  const void *ret_addr;
  const char *caller_module;
  size_t caller_offset;
} ErrorContext;

static void report_error(ErrorContext *ctx, const char *fmt, ...) {
  if(!do_report_error)
    return;

  va_list ap;
  va_start(ap, fmt);

  if(++num_errors > max_errors)  // Racy but ok
    return;

  if(!ctx->cmp_module) {
    // Lazily compute modules (no race!)

    const ProcMap *map_for_cmp = find_proc_map_for_addr(maps, nmaps, ctx->cmp_addr);
    ctx->cmp_module = map_for_cmp ? &map_for_cmp->name[0] : "<unknown>";
    ctx->cmp_offset = map_for_cmp ? ((const char *)ctx->cmp_addr - (const char *)map_for_cmp->begin_addr) : 0;

    const ProcMap *map_for_caller = find_proc_map_for_addr(maps, nmaps, ctx->ret_addr);
    ctx->caller_module = map_for_caller ? &map_for_caller->name[0] : "<unknown>";
    ctx->caller_offset = map_for_caller ? ((const char *)ctx->ret_addr - (const char *)map_for_caller->begin_addr) : 0;
  }

  char body[128];
  vsnprintf(body, sizeof(body), fmt, ap);

  char buf[256];

  char *full_msg = buf;
  size_t full_msg_size = sizeof(buf);
  size_t i;
  for(i = 0; i < 2; ++i) {
    // TODO: some parts of the message may be precomputed
    size_t need = snprintf(full_msg, full_msg_size, "%s[%ld]: %s: %s (comparison function %p (%s+%zx), called from %p (%s+%zx), cmdline is \"%s\")\n", proc_name, proc_pid, ctx->func, body, ctx->cmp_addr, ctx->cmp_module, ctx->cmp_offset, ctx->ret_addr, ctx->caller_module, ctx->caller_offset, proc_cmdline);
    if(i == 0 && need < sizeof(body))  // Did it fit to local buf?
      break;
    if(i == 0 && need >= sizeof(body)) {  // It didn't - go ahead and malloc
      full_msg_size = need + 1;
      full_msg = malloc(full_msg_size);
    }
  }

  // TODO: factor out generic printing
  if(!out)
    syslog(LOG_WARNING, "%s", full_msg);
  else
    fputs(full_msg, out);

  if(full_msg != buf)
    free(full_msg);
}

typedef int (*cmp_fun_t)(const void *, const void *);

static inline int sign(int x) {
  return x < 0 ? -1 : x > 0 ? 1 : 0;
}

// Check that comparator is stable and does not modify arguments
static void check_basic(ErrorContext *ctx, cmp_fun_t cmp, const char *key, const void *data, size_t n, size_t sz) {
  if(!(check_flags & CHECK_BASIC))
    return;

  const char *test_val = key ? key : data;
  size_t i0 = key ? 0 : 1;  // Avoid self-comparison
  size_t i;

  unsigned cs_test_val = key ? 0 : checksum(test_val, sz);

  // Check for modifying comparison functions
  for(i = i0; i < n; ++i) {
    const void *val = (const char *)data + i * sz;
    unsigned cs = checksum(val, sz);
    cmp(test_val, val);
    if(cs != checksum(val, sz)) {
      report_error(ctx, "comparison function modifies data");
      break;
    }

    if(!key && cs_test_val != checksum(test_val, sz)) {
      report_error(ctx, "comparison function modifies data");
      break;
    }

    if((check_flags & CHECK_REFLEXIVITY)
       && (!key || (check_flags & CHECK_GOOD_BSEARCH))) {
      cmp(val, val);
      if(cs != checksum(val, sz)) {
        report_error(ctx, "comparison function modifies data");
        break;
      }
    }
  }

  // Check for non-constant return value
  for(i = i0; i < n; ++i) {
    const void *val = (const char *)data + i * sz;
    if(cmp(test_val, val) != cmp(test_val, val)) {
      report_error(ctx, "comparison function returns unstable results");
      break;
    }

    if((check_flags & CHECK_REFLEXIVITY)
       && (!key || (check_flags & CHECK_GOOD_BSEARCH))) {
      if(cmp(val, val) != cmp(val, val)) {
        report_error(ctx, "comparison function returns unstable results");
        break;
      }
    }
  }
}

// Check that array is sorted
static void check_sorted(ErrorContext *ctx, cmp_fun_t cmp, const char *key, const void *data, size_t n, size_t sz) {
  if(!(check_flags & CHECK_SORTED))
    return;

  if(key) {
    int order = 1;
    size_t i;
    for(i = 0; i < n; ++i) {
      const void *val = (const char *)data + i * sz;
      int new_order = sign(cmp(key, val));
      if(new_order > order) {
        report_error(ctx, "processed array is not sorted at index %zd", i);
        return;  // Return to stop further error reporting
      }
      order = new_order;
    }
  }

  if(!key || (check_flags & CHECK_GOOD_BSEARCH)) {
    size_t i;
    for(i = 1; i < n; ++i) {
      const void *val = (const char *)data + i * sz;
      const void *prev = (const char *)val - sz;
      if(cmp(prev, val) > 0) {
        report_error(ctx, "processed array is not sorted at index %zd", i);
        break;
      }
    }
  }
}

// Check that ordering is total
static void check_total_order(ErrorContext *ctx, cmp_fun_t cmp, const char *key, const void *data, size_t n, size_t sz) {
  // Can check only good bsearch callbacks
  if(key && !(check_flags & CHECK_GOOD_BSEARCH))
    return;

  // TODO: 2 bits enough for status
  int8_t cmp_[32][32];
  n = n > 32 ? 32 : n;
  memset(cmp_, 0, sizeof(cmp_));

  size_t i, j, k;
  for(i = 0; i < n; ++i)
  for(j = 0; j < n; ++j) {
    const void *a = (const char *)data + i * sz;
    const void *b = (const char *)data + j * sz;
    if(i == j && !(check_flags & CHECK_REFLEXIVITY)) {
      // Do not call cmp(x,x) unless explicitly asked by user
      // because some projects assert on self-comparisons (e.g. GCC)
      cmp_[i][j] = 0;
      continue;
    }
    cmp_[i][j] = sign(cmp(a, b));
  }

  // Following axioms from http://mathworld.wolfram.com/StrictOrder.html

  // Totality by construction

  if(check_flags & CHECK_REFLEXIVITY) {
    for(i = 0; i < n; ++i) {
      // TODO: it may make sense to compare different but equal elements?
      if(0 != cmp_[i][i]) {
        report_error(ctx, "comparison function is not reflexive (returns non-zero for equal elements)");
        break;
      }
    }
  }

  if(check_flags & CHECK_SYMMETRY) {
    for(i = 0; i < n; ++i)
    for(j = 0; j < i; ++j) {
      if(cmp_[i][j] != -cmp_[j][i]) {
        report_error(ctx, "comparison function is not symmetric");
        goto sym_check_done;
      }
    }
  }
sym_check_done:

  if(check_flags & CHECK_TRANSITIVITY) {
    // FIXME: slow slow...
    for(i = 0; i < n; ++i)
    for(j = 0; j < i; ++j)
    for(k = 0; k < n; ++k) {
      if(cmp_[i][j] == cmp_[j][k] && cmp_[i][j] != cmp_[i][k]) {
        report_error(ctx, "comparison function is not transitive");
        goto trans_check_done;
      }
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
  if(num_errors < max_errors) {
    ErrorContext ctx = { __func__, cmp, 0, 0, __builtin_return_address(0), 0, 0 };
    check_basic(&ctx, cmp, key, data, n, sz);
    check_total_order(&ctx, cmp, key, data, n, sz);  // manpage does not require this but still
    check_sorted(&ctx, cmp, key, data, n, sz);
  }
  return _real(key, data, n, sz, cmp);
}

EXPORT void lfind(const void *key, const void *data, size_t *n, size_t sz, cmp_fun_t cmp) {
  MAYBE_INIT;
  GET_REAL(lfind);
  if(num_errors < max_errors) {
    ErrorContext ctx = { __func__, cmp, 0, 0, __builtin_return_address(0), 0, 0 };
    check_basic(&ctx, cmp, key, data, *n, sz);
    check_total_order(&ctx, cmp, key, data, *n, sz);
  }
  _real(key, data, n, sz, cmp);
}

EXPORT void lsearch(const void *key, void *data, size_t *n, size_t sz, cmp_fun_t cmp) {
  MAYBE_INIT;
  GET_REAL(lsearch);
  if(num_errors < max_errors) {
    ErrorContext ctx = { __func__, cmp, 0, 0, __builtin_return_address(0), 0, 0 };
    check_basic(&ctx, cmp, key, data, *n, sz);
    check_total_order(&ctx, cmp, key, data, *n, sz);
  }
  _real(key, data, n, sz, cmp);
}

EXPORT void qsort(void *data, size_t n, size_t sz, cmp_fun_t cmp) {
  MAYBE_INIT;
  GET_REAL(qsort);
  if(num_errors < max_errors) {
    ErrorContext ctx = { __func__, cmp, 0, 0, __builtin_return_address(0), 0, 0 };
    check_basic(&ctx, cmp, 0, data, n, sz);
    check_total_order(&ctx, cmp, 0, data, n, sz);
  }
  _real(data, n, sz, cmp);
}

// TODO: qsort_r

