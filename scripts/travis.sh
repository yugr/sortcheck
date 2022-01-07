#!/bin/sh

# Copyright 2019-2021 Yury Gribov
# 
# Use of this source code is governed by MIT license that can be
# found in the LICENSE.txt file.

set -eu

if test -n "${TRAVIS:-}" -o -n "${GITHUB_ACTIONS:-}"; then
  set -x
fi

cd $(dirname $0)/..

export ASAN_OPTIONS='detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1:strict_string_checks=1'

make "$@" clean all
make "$@" check

if test -n "${COVERAGE:-}"; then
  # Collect DLL coverage
  mv bin/*.gc[dn][ao] src
  gcov src/*.gcno
  # Upload coverage
  curl --retry 5 -s https://codecov.io/bash > codecov.bash
  bash codecov.bash -Z
fi
