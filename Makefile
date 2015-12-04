CC = gcc
CFLAGS = -fPIC -g -fvisibility=hidden -Wall -Wextra -Werror
ifeq (,$(DEBUG))
  CFLAGS += -O2
else
  CFLAGS += -O0
endif
LDFLAGS = -fPIC -shared
LIBS = -ldl

OBJS = bin/sortchecker.o

$(shell mkdir -p bin)

all: bin/libsortcheck.so

bin/libsortcheck.so: $(OBJS) Makefile
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

bin/%.o: src/%.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f bin/*

.PHONY: clean all

