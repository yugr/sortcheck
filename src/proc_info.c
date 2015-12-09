#include <proc_info.h>
#include <io.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

static char *read_field(char **p) {
  // Skip leading whites
  char *res = *p;
  while(*res == ' ')
    ++res;

  size_t len = strcspn(res, " \n");

  char *end = res + len;
  if(*end)
    *end++ = 0;

  // Skip closing bytes
//  while(*end == ' ')
//    ++end;
  *p = end;

  return res;
}

ProcMap *get_proc_maps(size_t *n) {
  FILE *p = fopen("/proc/self/maps", "rb");
  if(!p) {
    *n = 0;
    return 0;
  }

  size_t max_maps = 50;
  ProcMap *maps = malloc(max_maps * sizeof(ProcMap));

  char buf[512];
  size_t i = 0;
  while(fgets(buf, sizeof(buf), p)) { // FIXME: check if buffer was long enough
    // /proc/maps format:
    // address           perms offset  dev   inode       pathname
    // 00400000-00452000 r-xp 00000000 08:02 173521      /usr/bin/dbus-daemon

    const char *begin_addr = buf;
    char *dash = strchr(buf, '-');
    if(!dash)
      continue;
    *dash = 0;

    char *cur = dash + 1;
    char *end_addr = read_field(&cur);
    assert(end_addr);
    /*char *perms = */read_field(&cur);
    /*char *offset = */read_field(&cur);
    /*char *dev = */read_field(&cur);
    /*char *inode = */read_field(&cur);
    char *name = read_field(&cur);
    if(!name)
      name = "";

    if(i && 0 == strcmp(name, &maps[i - 1].name[0])) {
      // We already have entry for this file
      maps[i - 1].end_addr = (const void *)strtoull(end_addr, 0, 16);
    } else {
      maps[i].begin_addr = (const void *)strtoull(begin_addr, 0, 16);
      maps[i].end_addr = (const void *)strtoull(end_addr, 0, 16);
      strncpy(&maps[i].name[0], name, sizeof(maps[i].name));  // Hey, sizeof on arrays work!
      maps[i].name[sizeof(maps[i].name) - 1] = 0;  // Ugly...
      if(++i >= max_maps) {
        max_maps *= 2;
        maps = realloc(maps, max_maps * sizeof(ProcMap));
      }
    }
  } // while

  fclose(p);

  *n = i;
  return maps;
}

void get_proc_cmdline(char **pname, char **pcmdline) {
  *pname = 0;
  *pcmdline = 0;

  size_t size;
  char *cmdline = read_file("/proc/self/cmdline", &size);

  *pname = strdup(basename(cmdline));

  size_t i;
  for(i = 0; i < size; ++i) {
    if(!cmdline[i])
      cmdline[i] = ' ';
  }
  cmdline[size - 1] = 0;

  *pcmdline = cmdline;
}

const ProcMap *find_proc_map_for_addr(const ProcMap *maps, size_t n, const void *addr) {
  size_t i;
  for(i = 0; i < n; ++i)
    if(maps[i].begin_addr <= addr && addr < maps[i].end_addr)
      return &maps[i];
  return 0;
}

