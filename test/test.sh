#!/bin/sh

set -e

num_errors=0

error() {
  echo "$@"
  num_errors=$((num_errors + 1))
}

CHECK_PAT='// *CHECK *:'

for t in test/*.c; do
  gcc $t -Itest -o bin/a.out
  if ! LD_PRELOAD=bin/libsortcheck.so bin/a.out 2>bin/a.out.log; then
    error "$t: test exited with a non-zero exit code"
  elif ! grep -q "$CHECK_PAT" $t && test -s bin/a.out.log; then
    error "$t: non-empty stderr"
  else
    cat $t | sed -n 's!^\/\/ *CHECK *: *!!p' > bin/checks.txt
    failed=
    while read check; do
      if ! grep -q "$check" bin/a.out.log; then
        error "$t: output does not match pattern '$check'"
        failed=1
      fi
    done < bin/checks.txt
    if test -z "$failed"; then
      echo "$t: OK"
    fi
  fi
done
 
