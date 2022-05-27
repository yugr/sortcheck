# Copyright 2015-2022 Yury Gribov
# 
# Use of this source code is governed by MIT license that can be
# found in the LICENSE.txt file.

CC ?= gcc
DESTDIR ?= /usr/local

CPPFLAGS = -D_GNU_SOURCE -Iinclude
CFLAGS = -fPIC -g -fvisibility=hidden -Wall -Wextra -Werror
LDFLAGS = -fPIC -shared -Wl,--no-allow-shlib-undefined -Wl,--warn-common
LIBS = -ldl

ifneq (,$(COVERAGE))
  DEBUG = 1
  CFLAGS += --coverage -DNDEBUG
  LDFLAGS += --coverage
endif
ifeq (,$(DEBUG))
  CFLAGS += -O2
  LDFLAGS += -Wl,-O2
else
  CFLAGS += -O0
endif
ifneq (,$(ASAN))
  CFLAGS += -fsanitize=address -fsanitize-address-use-after-scope -U_FORTIFY_SOURCE -fno-common
  LDFLAGS += -Wl,--allow-shlib-undefined -fsanitize=address
endif
ifneq (,$(UBSAN))
	ifneq (,$(shell $(CC) --version | grep clang))
	# Isan is clang-only...
  CFLAGS += -fsanitize=undefined,integer -fno-sanitize-recover=undefined,integer
  LDFLAGS += -fsanitize=undefined,integer -fno-sanitize-recover=undefined,integer
  else
  CFLAGS += -fsanitize=undefined -fno-sanitize-recover=undefined
  LDFLAGS += -fsanitize=undefined -fno-sanitize-recover=undefined
	endif
  # Use Gold to avoid "unrecognized option --push-state--no-as-needed" from ld
  LDFLAGS += -fuse-ld=gold
endif

OBJS = bin/sortchecker.o bin/proc_info.o bin/checksum.o bin/io.o bin/flags.o bin/random.o

$(shell mkdir -p bin)

all: bin/libsortcheck.so

install:
	mkdir -p $(DESTDIR)
	install -D bin/libsortcheck.so $(DESTDIR)/lib
	install -D scripts/sortcheck $(DESTDIR)/bin

check:
	tests/test.sh

bin/libsortcheck.so: $(OBJS) bin/FLAGS Makefile
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

bin/%.o: src/%.c Makefile bin/FLAGS
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bin/FLAGS: FORCE
	if test x"$(CFLAGS) $(CXXFLAGS) $(LDFLAGS)" != x"$$(cat $@)"; then \
		echo "$(CFLAGS) $(CXXFLAGS) $(LDFLAGS)" > $@; \
	fi

# TODO: add depend target

help:
	@echo "Common targets:"
	@echo "  all        Build all executables and scripts"
	@echo "  clean      Clean all build files and temps."
	@echo "  help       Print help on build options."
	@echo '  install    Install to $$DESTDIR (default is /usr).'
	@echo ""
	@echo "Less common:"
	@echo "  check      Run regtests."
	@echo ""
	@echo "Build options:"
	@echo "  DESTDIR=path  Specify installation root."
	@echo "  DEBUG=1       Build debug version of code."
	@echo "  ASAN=1        Build with ASan checks."
	@echo "  UBSAN=1       Build with UBSan checks."

clean:
	rm -f bin/*
	find -name \*.gcov -o -name \*.gcno -o -name \*.gcda | xargs rm -rf

.PHONY: clean all install check FORCE help

