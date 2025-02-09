#!/bin/sh

# Copyright 2015-2024 Yury Gribov
# 
# Use of this source code is governed by MIT license that can be
# found in the LICENSE.txt file.

set -eu

if test -n "${TRAVIS:-}" -o -n "${GITHUB_ACTIONS:-}"; then
  set -x
fi

cd $(dirname $0)/..

num_errors=0

error() {
  echo "$@"
  num_errors=$((num_errors + 1))
}

has_option() {
  grep -q "// *$2 *:" $1
}

get_multi_option() {
  cat $1 | sed -n 's!.*\/\/ *'$2' *: *!!p' | paste -sd, -
}

get_option() {
  cat $1 | sed -n 's!.*\/\/ *'$2' *: *!!p'
}

get_syslog() {
  if test -r /var/log/syslog; then
    cat /var/log/syslog
  elif which journalctl >/dev/null 2>&1; then
    journalctl -q
  else
    :
  fi | grep -a a.out.*qsort || true
}

# From https://github.com/google/sanitizers/wiki/AddressSanitizer
export ASAN_OPTIONS=detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1:strict_string_checks=1

# Get rid of "ASan runtime does not come first in initial library list"
ASAN_OPTIONS=verify_asan_link_order=0:$ASAN_OPTIONS

CC=${CC:-cc}

CFLAGS=
if uname | grep -q Darwin; then
  # DYLD_FORCE_FLAT_NAMESPACE is not enough (due to System Integrity Protection?)
  CFLAGS="$CFLAGS -flat_namespace"
fi

if readelf -sDW bin/libsortcheck.so | grep -q __asan_init; then
  if $CC --version | grep -q clang; then
    ASAN_RT_LIB=libclang_rt.asan-x86_64.so
  else  # clang
    ASAN_RT_LIB=libasan.so
  fi
  ASAN_PRELOAD=$($CC --print-file-name=$ASAN_RT_LIB)
else
  ASAN_PRELOAD=
fi

VALGRIND=${VALGRIND:-}
if test -n "${VALGRIND:-}"; then
  VALGRIND='valgrind -q --error-exitcode=1'
fi

has_feature() {
  case "$1" in
    asan)
      test -n "$ASAN_PRELOAD"
      ;;
    bsd)
      uname | grep -q 'BSD\|Darwin'
      ;;
    mac)
      uname | grep -q Darwin
      ;;
    netbsd)
      uname | grep -q NetBSD
      ;;
    openbsd)
      uname | grep -q OpenBSD
      ;;
    proc)
      test -d /proc && test -r /proc
      ;;
    syslog)
      test -r /var/log/syslog || which journalctl >/dev/null 2>&1
      ;;
    *)
      echo >&2 "$t: unknown feature: $d"
      exit 1
      ;;
  esac
}

for t in tests/*.c; do
  failed=

  if has_option $t REQUIRE; then
    SKIP=
    for d in $(get_multi_option $t REQUIRE | sed -e 's/, */ /g'); do
      if ! has_feature $d; then
        SKIP=1
        break
      fi
    done
    if test -n "$SKIP"; then
      echo "$t: skipped"
      continue
    fi
  fi

  if has_option $t SKIP; then
    SKIP=
    for d in $(get_multi_option $t SKIP | sed -e 's/, */ /g'); do
      if has_feature $d; then
        SKIP=1
        break
      fi
    done
    if test -n "$SKIP"; then
      echo "$t: skipped"
      continue
    fi
  fi

  rm -f bin/a.out
  if ! $CC $t -Itest $CFLAGS $(get_option $t CFLAGS) -o bin/a.out; then
    error "$t: compilation failed"
    failed=1
  fi

  if has_option $t OPTS; then
    export SORTCHECK_OPTIONS=$(get_option $t OPTS)
  else
    unset SORTCHECK_OPTIONS
  fi

  if has_option $t CMDLINE; then
    ARGS=$(get_option $t CMDLINE)
  else
    ARGS=
  fi

  get_syslog > bin/syslog.bak

  set +e
  (
    export LD_PRELOAD=${LD_PRELOAD:+$LD_PRELOAD:}${ASAN_PRELOAD:+$ASAN_PRELOAD:}bin/libsortcheck.so
    export DYLD_INSERT_LIBRARIES=${DYLD_INSERT_LIBRARIES:+$DYLD_INSERT_LIBRARIES:}${ASAN_PRELOAD:+$ASAN_PRELOAD:}bin/libsortcheck.so
    export DYLD_FORCE_FLAT_NAMESPACE=1
    $VALGRIND bin/a.out $ARGS 2>bin/a.out.log
  )
  rc=$?
  set -e

  if test -n "${TRAVIS:-}" -o -n "${GITHUB_ACTIONS:-}"; then
    cat bin/a.out.log
  fi

  if test $rc != 0; then
    error "$t: test exited with a non-zero exit code"
    failed=1
  elif ! has_option $t CHECK && ! has_option $t CHECK-NOT && test -s bin/a.out.log; then
    error "$t: non-empty stderr"
    failed=1
  else
    get_syslog > bin/syslog

    get_option $t CHECK > bin/checks.txt
    while read check; do
      if ! grep -q "$check" bin/a.out.log; then
        error "$t: output does not match pattern '$check'"
        failed=1
      fi
    done < bin/checks.txt

    get_option $t CHECK-NOT > bin/checknots.txt
    while read checknot; do
      if grep -q "$checknot" bin/a.out.log; then
        error "$t: output matches prohibited pattern '$checknot'"
        failed=1
      fi
    done < bin/checknots.txt
  fi

  if has_option $t SYSLOG || has_option $t SYSLOG-NOT; then
    get_option $t SYSLOG > bin/syslog_checks.txt
    while read check; do
      if ! comm -13 bin/syslog.bak bin/syslog | grep -q "$check"; then
        error "$t: syslog does not match pattern '$check'"
        failed=1
      fi
    done < bin/syslog_checks.txt

    get_option $t SYSLOG-NOT > bin/syslog_checknots.txt
    while read checknot; do
      if comm -13 bin/syslog.bak bin/syslog | grep -q "$check"; then
        error "$t: syslog matches prohibited pattern '$check'"
        failed=1
      fi
    done < bin/syslog_checknots.txt
  fi

  if test -z "$failed"; then
    echo "$t: OK"
  else
    head -10 bin/a.out.log
  fi
done

echo "Total: $num_errors errors"
test $num_errors = 0 || exit 1
