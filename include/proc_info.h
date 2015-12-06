#ifndef PROC_INFO_H
#define PROC_INFO_H

#include <stdint.h>

typedef struct {
  const void *begin_addr, *end_addr;
  char name[128];
} ProcMap;

ProcMap *get_proc_maps(size_t *n);

void get_proc_cmdline(char **pname, char **pcmdline);

#endif