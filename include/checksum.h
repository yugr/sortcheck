#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stdlib.h>

// Fletcher's checksum
unsigned checksum(const void *data, size_t sz);

#endif
