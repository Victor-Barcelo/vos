#ifndef VOS_OLIVE_H
#define VOS_OLIVE_H

// Convenience wrapper so user programs can `#include <olive.h>`.
// Upstream is a single-file library; implementation is enabled by defining
// `OLIVEC_IMPLEMENTATION` in exactly one translation unit before including it.
#ifndef OLIVECDEF
#define OLIVECDEF extern
#endif
#include "olive.c"

#endif
