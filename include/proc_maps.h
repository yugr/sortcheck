#ifndef PROC_MAPS_H
#define PROC_MAPS_H

#include <stdint.h>

typedef struct {
  const void *begin_addr, *end_addr;
  char name[128];
} ProcMap;

ProcMap *get_proc_maps(size_t *n);

#endif
