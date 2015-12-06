CC = gcc-4.9
CPPFLAGS = -D_GNU_SOURCE -Iinclude
CFLAGS = -fPIC -g -fvisibility=hidden -Wall -Wextra -Werror
LDFLAGS = -fPIC -shared -Wl,--no-allow-shlib-undefined
ifeq (,$(DEBUG))
  CFLAGS += -O2
  LDFLAGS += -Wl,-O2
else
  CFLAGS += -O0
endif
ifneq (,$(SANITIZE))
  CFLAGS += -fsanitize=address
  LDFLAGS += -Wl,--allow-shlib-undefined -fsanitize=address
endif
LIBS = -ldl

OBJS = bin/sortchecker.o bin/proc_info.o bin/checksum.o

$(shell mkdir -p bin)

all: bin/libsortcheck.so

bin/libsortcheck.so: $(OBJS) Makefile
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

bin/%.o: src/%.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f bin/*

.PHONY: clean all

