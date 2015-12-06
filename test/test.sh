#!/bin/sh

set -e

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

CHECK_PAT='// *CHECK *:'
OPTS_PAT='// *OPTS *:'

for t in test/*.c; do
  gcc $t -Itest -o bin/a.out
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
  if ! LD_PRELOAD=bin/libsortcheck.so bin/a.out $ARGS 2>bin/a.out.log; then
    error "$t: test exited with a non-zero exit code"
  elif ! has_option $t CHECK && ! has_option $t CHECK-NOT && test -s bin/a.out.log; then
    error "$t: non-empty stderr"
  else
    failed=

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

    if test -z "$failed"; then
      echo "$t: OK"
    fi
  fi
done

echo "Total: $num_errors errors"
 
