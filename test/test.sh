#!/bin/sh

# Copyright 2015-2016 Yury Gribov
# 
# Use of this source code is governed by MIT license that can be
# found in the LICENSE.txt file.

set -eu

cd $(dirname $0)/..

num_errors=0

error() {
  echo "$@"
  num_errors=$((num_errors + 1))
}

has_option() {
  grep -q "// *$2 *:" $1
}

get_option() {
  cat $1 | sed -n 's!.*\/\/ *'$2' *: *!!p'
}

get_syslog() {
  if test -n "${TRAVIS:-}"; then
    :
  elif test -f /var/log/syslog; then
    cat /var/log/syslog
  else
    journalctl -q
  fi
}

# From https://github.com/google/sanitizers/wiki/AddressSanitizer
export ASAN_OPTIONS=strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1

# Get rid of "ASan runtime does not come first in initial library list"
ASAN_OPTIONS=verify_asan_link_order=0:$ASAN_OPTIONS

for t in test/*.c; do
  failed=

  if ! gcc $t -Itest $(get_option $t CFLAGS) -o bin/a.out; then
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
  if has_option $t SYSLOG && test -n "${TRAVIS:-}"; then
    echo "Skipping syslog test $t in Travis environment"
    continue
  fi

  get_syslog > bin/syslog.bak

  if ! LD_PRELOAD=${LD_PRELOAD:+$LD_PRELOAD:}bin/libsortcheck.so bin/a.out $ARGS 2>bin/a.out.log; then
    error "$t: test exited with a non-zero exit code"
    failed=1
  elif ! has_option $t CHECK && ! has_option $t CHECK-NOT && test -s bin/a.out.log; then
    error "$t: non-empty stderr"
    failed=1
  else
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
        error "$t: output matches prohibited pattern '$check'"
        failed=1
      fi
    done < bin/checknots.txt
  fi

  if has_option $t SYSLOG || has_option $t SYSLOG-NOT; then
    get_option $t SYSLOG > bin/syslog_checks.txt
    while read check; do
      get_syslog > bin/syslog
      if ! comm -13 bin/syslog.bak bin/syslog | grep -q "$check"; then
        error "$t: syslog does not match pattern '$check'"
        failed=1
      fi
    done < bin/syslog_checks.txt

    get_option $t SYSLOG-NOT > bin/syslog_checknots.txt
    while read checknot; do
      get_syslog > bin/syslog
      if comm -13 bin/syslog.bak bin/syslog | grep -q "$check"; then
        error "$t: syslog matches prohibited pattern '$check'"
        failed=1
      fi
    done < bin/syslog_checknots.txt
  fi

  if test -z "$failed"; then
    echo "$t: OK"
  fi
done

echo "Total: $num_errors errors"
test $num_errors = 0 || exit 1
