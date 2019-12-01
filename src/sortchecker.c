/*
 * Copyright 2015-2018 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#include <checksum.h>
#include <proc_info.h>
#include <flags.h>
#include <io.h>
#include <platform.h>

// Predeclare exported functions to please Clang.

typedef int (*cmp_fun_t)(const void *, const void *);
typedef int (*cmp_r_fun_t)(const void *, const void *, void *);

EXPORT void *bsearch(const void *key, const void *data, size_t n, size_t sz, cmp_fun_t cmp);
EXPORT void lfind(const void *key, const void *data, size_t *n, size_t sz, cmp_fun_t cmp);
EXPORT void lsearch(const void *key, void *data, size_t *n, size_t sz, cmp_fun_t cmp);
EXPORT void qsort(void *data, size_t n, size_t sz, cmp_fun_t cmp);
EXPORT void qsort(void *data, size_t n, size_t sz, cmp_fun_t cmp);
EXPORT void heapsort(void *data, size_t n, size_t sz, cmp_fun_t cmp);
EXPORT void mergesort(void *data, size_t n, size_t sz, cmp_fun_t cmp);
EXPORT void qsort_r(void *data, size_t n, size_t sz, cmp_r_fun_t cmp, void *arg);
EXPORT void *dlopen(const char *filename, int flag);
EXPORT int dlclose(void *handle);

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>

#include <dlfcn.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/types.h>
#include <unistd.h>

// Runtime options
static FILE *out;
static Flags flags = {
  /*debug*/ 0,
  /*report_error*/ 1,
  /*print_to_syslog*/ 0,
  /*raise*/ 0,
  /*max_errors*/ 10,
  /*sleep*/ 0,
  /*checks*/ CHECK_DEFAULT,
  /*out_filename*/ 0
};

typedef struct ProcMapNode_ {
  ProcMap *maps;
  size_t nmaps;
  struct ProcMapNode_ *next;
  int dlopen_gen;
} ProcMapNode;

// Other pieces of state
static volatile int init_in_progress = 0, init_done = 0;
static ProcMapNode maps_first, *maps_head = &maps_first;
static int dlopen_gen;
static char *proc_name, *proc_cmdline;
static unsigned num_errors = 0;
static long proc_pid = -1;
static const void **reported_errors;

static void fini(void) {
  // FIXME: do we really need to release this stuff?

  ProcMapNode *maps_head_ = maps_head;
  maps_head = &maps_first;
  barrier();
  for(; maps_head_ != &maps_first; ) {
    ProcMapNode *old = maps_head_;
    maps_head_ = maps_head_->next;
    free(old->maps);
    free(old);
  }

  if(proc_cmdline)
    free(proc_cmdline);
  if(proc_name)
    free(proc_name);
  if(reported_errors)
    free(reported_errors);
}

static void update_maps() {
  int gen = dlopen_gen;
  barrier();

  assert(gen >= maps_head->dlopen_gen);
  if(gen == maps_head->dlopen_gen)
    return;

  ProcMapNode *new = malloc(sizeof(ProcMapNode));
  new->maps = get_proc_maps(&new->nmaps);
  new->next = maps_head;
  new->dlopen_gen = gen;
  maps_head = new;  // TODO: CAS?

  if(flags.debug) {
    fprintf(out, "Process map (gen %d):\n", new->dlopen_gen);
    size_t i;
    for(i = 0; i < new->nmaps; ++i) {
      const ProcMap *m = &new->maps[i];
      fprintf(out, "  %50s: %p-%p\n", &m->name[0], m->begin_addr, m->end_addr);
    }
  }
}

static void init(void) {
  if(init_done)
    return;

  // TODO: proper atomics here
  if(init_in_progress) {
    // This is a workaround for recursive deadlock with libcowdancer:
    // (gdb) bt
    // #0  init () at src/sortchecker.c:105
    // #1  0x00007f4c2bc125c4 in bsearch (key=0x7fff1b02d130, data=0x7f4c2c1e1010, n=20721, sz=16, cmp=0x7f4c2be177d0 <compare_ilist>)
    //     at src/sortchecker.c:468
    // #2  0x00007f4c2be16e42 in ?? () from /usr/lib/cowdancer/libcowdancer.so
    // #3  0x00007f4c2be173e7 in fopen () from /usr/lib/cowdancer/libcowdancer.so
    // #4  0x00007f4c2bc11806 in init () at src/sortchecker.c:197
    // #5  0x00007f4c2bc129a0 in qsort (data=0x6f0e20 <static_shell_builtins>, n=76, sz=48, cmp=0x4746a0) at src/sortchecker.c:502
    // #6  0x0000000000420c9e in ?? ()
    // #7  0x000000000041f2cd in main ()
    //
    // FIXME: we should be able to detect recursion
#if 0
    while(!init_done)
      barrier();
    // TODO: sleep?
#endif

    return;
  }

  init_in_progress = 1;
  barrier();

  char *opts;
  if((opts = read_file("/SORTCHECK_OPTIONS", 0))) {
    if(!parse_flags(opts, &flags)) {
      free(opts);
      exit(1);
    }
    free(opts);
  }

  if((opts = getenv("SORTCHECK_OPTIONS"))) {
    opts = strdup(opts);
    if(!parse_flags(opts, &flags)) {
      free(opts);
      exit(1);
    }
    free(opts);
  }

  if(flags.print_to_syslog && flags.out_filename) {
    fprintf(stderr, "sortcheck: both print_to_syslog and print_to_file were specified\n");
    exit(1);
  } else if(flags.print_to_syslog) {
    openlog("", 0, LOG_USER);
  } else if(flags.out_filename) {
    if(!(out = fopen(flags.out_filename, "ab"))) {
      fprintf(stderr, "sortcheck: failed to open %s for writing: errno %d: ", flags.out_filename, errno);
      perror(0);
      exit(1);
    }
  } else
    out = stderr;

  get_proc_cmdline(&proc_name, &proc_cmdline);
  dlopen_gen = 1; // Will cause recalculation of mappings

  proc_pid = (long)getpid();

  reported_errors = calloc(flags.max_errors, sizeof(void *));

  atexit(fini);

  // TODO: proper atomics here
  barrier();
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
  int found_error;
} ErrorContext;

static void report_error(ErrorContext *ctx, const char *fmt, ...) {
  // Racy but ok
  size_t i;
  for(i = 0; i < flags.max_errors; ++i) {
    if (!reported_errors[i]) {
      reported_errors[i] = ctx->cmp_addr;
      break;
    }
  }
  if(i == flags.max_errors)
    return;

  // Increment global counter on first error in current invocation
  if(!ctx->found_error) {
    ctx->found_error = 1;
    ++num_errors;
  }

  if(!flags.report_error)
    return;

  va_list ap;
  va_start(ap, fmt);

  if(!ctx->cmp_module) {
    // Lazily compute modules (no race!)

    update_maps();

    const ProcMap *map_for_cmp = find_proc_map_for_addr(maps_head->maps, maps_head->nmaps, ctx->cmp_addr);
    if(map_for_cmp) {
      ctx->cmp_module = &map_for_cmp->name[0];
      ctx->cmp_offset = (size_t)ctx->cmp_addr;
      if(strstr(ctx->cmp_module, ".so"))  // FIXME: how to detect PIE?
        ctx->cmp_offset -= (size_t)map_for_cmp->begin_addr;
    } else {
      ctx->cmp_module = "<unknown>";
      ctx->cmp_offset = 0;
    }

    const ProcMap *map_for_caller = find_proc_map_for_addr(maps_head->maps, maps_head->nmaps, ctx->ret_addr);
    if(map_for_caller) {
      ctx->caller_module = &map_for_caller->name[0];
      ctx->caller_offset = (size_t)ctx->ret_addr;
      if(strstr(ctx->caller_module, ".so"))  // FIXME: how to detect PIE?
        ctx->caller_offset -= (size_t)map_for_caller->begin_addr;
    } else {
      ctx->caller_module = "<unknown>";
      ctx->caller_offset = 0;
    }
  }

  char body[128];
  vsnprintf(body, sizeof(body), fmt, ap);

  char buf[256];

  char *full_msg = buf;
  size_t full_msg_size = sizeof(buf);
  for(i = 0; i < 2; ++i) {
    // TODO: some parts of the message may be precomputed
    size_t need = snprintf(full_msg, full_msg_size, "%s[%ld]: %s: %s (comparison function %p (%s+0x%zx), called from %p (%s+0x%zx), cmdline is \"%s\")\n", proc_name, proc_pid, ctx->func, body, ctx->cmp_addr, ctx->cmp_module, ctx->cmp_offset, ctx->ret_addr, ctx->caller_module, ctx->caller_offset, proc_cmdline);
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

  if(flags.sleep)
    sleep(flags.sleep);

  if(flags.raise)
    raise(SIGTRAP);

  va_end(ap);
}

typedef struct {
  void *cmp;
  void *arg;
  int is_reentrant;
} Comparator;

static inline int cmp_eval(const Comparator *cmp, const void *a, const void *b) {
  return cmp->is_reentrant ? ((cmp_r_fun_t)cmp->cmp)(a, b, cmp->arg) : ((cmp_fun_t)cmp->cmp)(a, b);
}

static inline int sign(int x) {
  return x < 0 ? -1 : x > 0 ? 1 : 0;
}

// Check that comparator is stable and does not modify arguments
static void check_basic(ErrorContext *ctx, const Comparator *cmp, const char *key, const void *data, size_t n, size_t sz) {
  if(!(flags.checks & CHECK_BASIC))
    return;

  const char *test_val = key ? key : data;
  size_t i0 = key ? 0 : 1;  // Avoid self-comparison
  size_t i;

  unsigned cs_test_val = key ? 0 : checksum(test_val, sz);

  // Check for modifying comparison functions
  for(i = i0; i < n; ++i) {
    const void *val = (const char *)data + i * sz;
    unsigned cs = checksum(val, sz);
    cmp_eval(cmp, test_val, val);
    if(cs != checksum(val, sz)) {
      report_error(ctx, "comparison function modifies data");
      break;
    }

    if(!key && cs_test_val != checksum(test_val, sz)) {
      report_error(ctx, "comparison function modifies data");
      break;
    }

    if((flags.checks & CHECK_REFLEXIVITY)
       && (!key || (flags.checks & CHECK_GOOD_BSEARCH))) {
      cmp_eval(cmp, val, val);
      if(cs != checksum(val, sz)) {
        report_error(ctx, "comparison function modifies data");
        break;
      }
    }
  }

  // Check for non-constant return value
  for(i = i0; i < n; ++i) {
    const void *val = (const char *)data + i * sz;
    if(cmp_eval(cmp, test_val, val) != cmp_eval(cmp, test_val, val)) {
      report_error(ctx, "comparison function returns unstable results");
      break;
    }

    if((flags.checks & CHECK_REFLEXIVITY)
       && (!key || (flags.checks & CHECK_GOOD_BSEARCH))) {
      if(cmp_eval(cmp, val, val) != cmp_eval(cmp, val, val)) {
        report_error(ctx, "comparison function returns unstable results");
        break;
      }
    }
  }
}

static void check_uniqueness(ErrorContext *ctx, const Comparator *cmp, const void *data, size_t n, size_t sz) {
  if(!(flags.checks & CHECK_UNIQUE))
    return;

  size_t i;
  for(i = 1; i < n; ++i) {
    const void *val = (const char *)data + i*sz;
    const void *prev = (const char *)val - sz;
    if(!cmp_eval(cmp, val, prev) && 0 != memcmp(prev, val, sz)) {
      report_error(ctx, "comparison function compares different objects as equal at index %zd", i);
      return;  // Stop further reporting
    }
  }
}

// Check that array is sorted
static void check_sorted(ErrorContext *ctx, const Comparator *cmp, const char *key, const void *data, size_t n, size_t sz) {
  if(!(flags.checks & CHECK_SORTED))
    return;

  if(key) {
    int order = 1;
    size_t i;
    for(i = 0; i < n; ++i) {
      const void *val = (const char *)data + i * sz;
      int new_order = sign(cmp_eval(cmp, key, val));
      if(new_order > order) {
        report_error(ctx, "processed array is not sorted at index %zd", i);
        return;  // Return to stop further error reporting
      }
      order = new_order;
    }
  }

  if(!key || (flags.checks & CHECK_GOOD_BSEARCH)) {
    size_t i;
    for(i = 1; i < n; ++i) {
      const void *val = (const char *)data + i * sz;
      const void *prev = (const char *)val - sz;
      if(cmp_eval(cmp, prev, val) > 0) {
        report_error(ctx, "processed array is not sorted at index %zd", i);
        break;
      }
    }
  }
}

// Check that ordering is total
static void check_total_order(ErrorContext *ctx, const Comparator *cmp, const char *key, const void *data, size_t n, size_t sz) {
  // Can check only good bsearch callbacks
  if(key && !(flags.checks & CHECK_GOOD_BSEARCH))
    return;

  // TODO: 2 bits enough for status
  // TODO: randomize selection of sub-array (and print seed in error message for repro)
  int8_t cmp_[32][32];
  n = n > 32 ? 32 : n;
  memset(cmp_, 0, sizeof(cmp_));

  size_t i, j, k;
  for(i = 0; i < n; ++i)
  for(j = 0; j < n; ++j) {
    const void *a = (const char *)data + i * sz;
    const void *b = (const char *)data + j * sz;
    if(i == j && !(flags.checks & CHECK_REFLEXIVITY)) {
      // Do not call cmp(x,x) unless explicitly asked by user
      // because some projects assert on self-comparisons (e.g. GCC)
      cmp_[i][j] = 0;
      continue;
    }
    cmp_[i][j] = sign(cmp_eval(cmp, a, b));
  }

  // Following axioms from http://mathworld.wolfram.com/StrictOrder.html

  // Totality by construction

  if(flags.checks & CHECK_REFLEXIVITY) {
    for(i = 0; i < n; ++i) {
      // TODO: it may make sense to compare different but equal elements?
      if(0 != cmp_[i][i]) {
        report_error(ctx, "comparison function is not reflexive (returns non-zero for equal elements)");
        break;
      }
    }
  }

  if(flags.checks & CHECK_SYMMETRY) {
    for(i = 0; i < n; ++i)
    for(j = 0; j < i; ++j) {
      if(cmp_[i][j] != -cmp_[j][i]) {
        report_error(ctx, "comparison function is not symmetric");
        goto sym_check_done;
      }
    }
  }
sym_check_done:

  if(flags.checks & CHECK_TRANSITIVITY) {
    // FIXME: slow slow...
    for(i = 0; i < n; ++i)
    for(j = 0; j < i; ++j)
    for(k = 0; k < n; ++k) {
      // Don't compare element to itself unless requested by user
      if((i == k || j == k) && !(flags.checks & CHECK_REFLEXIVITY))
        continue;
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
    if(flags.debug)                                          \
      fprintf(out, "Hello from %s interceptor!\n", #sym); \
  }

#define MAYBE_INIT do {  \
  if(!init_done) init(); \
} while(0)

static int suppress_errors(const void *cmp) {
  if(init_in_progress || num_errors >= flags.max_errors)
    return 1;
  // Uniqueness check (racy but ok)
  size_t i;
  for(i = 0; i < flags.max_errors; ++i) {
    if (reported_errors[i] == cmp)
      return 1;
  }
  return 0;
}

EXPORT void *bsearch(const void *key, const void *data, size_t n, size_t sz, cmp_fun_t cmp) {
  MAYBE_INIT;
  GET_REAL(bsearch);
  if(!suppress_errors(cmp)) {
    ErrorContext ctx = { __func__, cmp, 0, 0, __builtin_return_address(0), 0, 0, 0 };
    Comparator c = { cmp, 0, 0 };
    check_basic(&ctx, &c, key, data, n, sz);
    check_total_order(&ctx, &c, key, data, n, sz);  // manpage does not require this but still
    check_sorted(&ctx, &c, key, data, n, sz);
  }
  return _real(key, data, n, sz, cmp);
}

EXPORT void lfind(const void *key, const void *data, size_t *n, size_t sz, cmp_fun_t cmp) {
  MAYBE_INIT;
  GET_REAL(lfind);
  ErrorContext ctx = { __func__, cmp, 0, 0, __builtin_return_address(0), 0, 0, 0 };
  Comparator c = { cmp, 0, 0 };
  int suppress_errors_ = suppress_errors(cmp);
  if(!suppress_errors_) {
    check_basic(&ctx, &c, key, data, *n, sz);
    check_total_order(&ctx, &c, key, data, *n, sz);
  }
  _real(key, data, n, sz, cmp);
  if(!suppress_errors_)
    check_uniqueness(&ctx, &c, data, *n, sz);
}

EXPORT void lsearch(const void *key, void *data, size_t *n, size_t sz, cmp_fun_t cmp) {
  MAYBE_INIT;
  GET_REAL(lsearch);
  ErrorContext ctx = { __func__, cmp, 0, 0, __builtin_return_address(0), 0, 0, 0 };
  Comparator c = { cmp, 0, 0 };
  int suppress_errors_ = suppress_errors(cmp);
  if(!suppress_errors_) {
    check_basic(&ctx, &c, key, data, *n, sz);
    check_total_order(&ctx, &c, key, data, *n, sz);
  }
  _real(key, data, n, sz, cmp);
  if(!suppress_errors_)
    check_uniqueness(&ctx, &c, data, *n, sz);
}

static inline void qsort_common(void *data, size_t n, size_t sz, cmp_fun_t cmp,
                         void (*real)(void *, size_t n, size_t sz, cmp_fun_t cmp),
                         ErrorContext *ctx) {
  Comparator c = { cmp, 0, 0 };
  int suppress_errors_ = suppress_errors(cmp);
  if(!suppress_errors_) {
    check_basic(ctx, &c, 0, data, n, sz);
    check_total_order(ctx, &c, 0, data, n, sz);
  }
  real(data, n, sz, cmp);
  if(!suppress_errors_)
    check_uniqueness(ctx, &c, data, n, sz);
}

EXPORT void qsort(void *data, size_t n, size_t sz, cmp_fun_t cmp) {
  MAYBE_INIT;
  GET_REAL(qsort);
  ErrorContext ctx = { __func__, cmp, 0, 0, __builtin_return_address(0), 0, 0, 0 };
  qsort_common(data, n, sz, cmp, _real, &ctx);
}

// BSD extension
EXPORT void heapsort(void *data, size_t n, size_t sz, cmp_fun_t cmp) {
  MAYBE_INIT;
  GET_REAL(heapsort);
  ErrorContext ctx = { __func__, cmp, 0, 0, __builtin_return_address(0), 0, 0, 0 };
  qsort_common(data, n, sz, cmp, _real, &ctx);
}

// BSD extension
EXPORT void mergesort(void *data, size_t n, size_t sz, cmp_fun_t cmp) {
  MAYBE_INIT;
  GET_REAL(mergesort);
  ErrorContext ctx = { __func__, cmp, 0, 0, __builtin_return_address(0), 0, 0, 0 };
  qsort_common(data, n, sz, cmp, _real, &ctx);
}

EXPORT void qsort_r(void *data, size_t n, size_t sz, cmp_r_fun_t cmp, void *arg) {
  MAYBE_INIT;
  GET_REAL(qsort_r);
  ErrorContext ctx = { __func__, cmp, 0, 0, __builtin_return_address(0), 0, 0, 0 };
  Comparator c = { cmp, arg, 1 };
  int suppress_errors_ = suppress_errors(cmp);
  if (!suppress_errors_) {
    check_basic(&ctx, &c, 0, data, n, sz);
    check_total_order(&ctx, &c, 0, data, n, sz);
  }
  _real(data, n, sz, cmp, arg);
  if (!suppress_errors_)
    check_uniqueness(&ctx, &c, data, n, sz);
}

EXPORT void *dlopen(const char *filename, int flag) {
  GET_REAL(dlopen);
  void *res = _real(filename, flag);
  ++dlopen_gen; // TODO: proper atomics here
  return res;
}

EXPORT int dlclose(void *handle) {
  GET_REAL(dlclose);
  int res = _real(handle);
  ++dlopen_gen; // TODO: proper atomics here
  return res;
}
