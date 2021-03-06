# Copyright 2015-2019 Yury Gribov
# 
# Use of this source code is governed by MIT license that can be
# found in the LICENSE.txt file.

CC ?= gcc
CPPFLAGS = -D_GNU_SOURCE -Iinclude
CFLAGS = -fPIC -g -fvisibility=hidden -Wall -Wextra -Werror
LDFLAGS = -fPIC -shared -Wl,--no-allow-shlib-undefined
ifeq (,$(DEBUG))
  CFLAGS += -O2
  LDFLAGS += -Wl,-O2
else
  CFLAGS += -O0
endif
ifneq (,$(ASAN))
  CFLAGS += -fsanitize=address -fsanitize-address-use-after-scope
  LDFLAGS += -Wl,--allow-shlib-undefined -fsanitize=address
endif
ifneq (,$(UBSAN))
  CFLAGS += -fsanitize=undefined -fno-sanitize-recover=undefined
  LDFLAGS += -fsanitize=undefined
  # Use Gold to avoid "unrecognized option --push-state--no-as-needed" from ld
  LDFLAGS += -fuse-ld=gold
endif
ifneq (,$(COVERAGE))
  DEBUG = 1
  CFLAGS += -O0 -DNDEBUG --coverage
  LDFLAGS += --coverage
endif
LIBS = -ldl

DESTDIR = /usr

OBJS = bin/sortchecker.o bin/proc_info.o bin/checksum.o bin/io.o bin/flags.o

$(shell mkdir -p bin)

all: bin/libsortcheck.so

install:
	install -D bin/libsortcheck.so $(DESTDIR)/lib
	install -D scripts/sortcheck $(DESTDIR)/bin

check:
	test/test.sh

bin/libsortcheck.so: $(OBJS) bin/FLAGS Makefile
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

bin/%.o: src/%.c Makefile bin/FLAGS
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bin/FLAGS: FORCE
	if test x"$(CFLAGS) $(LDFLAGS)" != x"$$(cat $@)"; then \
		echo "$(CFLAGS) $(LDFLAGS)" > $@; \
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
	rm -f bin/* *.gcov

.PHONY: clean all install check FORCE help

