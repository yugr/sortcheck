#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef __GNUC__
#define EXPORT __attribute__((visibility("default")))

// TODO: proper atomics here
#define barrier() asm("");

#else
#error "Unknown compiler"
#endif

#endif

