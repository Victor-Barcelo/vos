#ifndef VOS_SYS_SYSMACROS_H
#define VOS_SYS_SYSMACROS_H

#include <sys/types.h>

// Minimal Linux-style device number helpers.
// VOS does not currently expose real device nodes, but some ports (e.g. sbase ls)
// expect these macros to exist.

#ifndef major
#define major(dev) ((int)(((dev) >> 8) & 0xFF))
#endif

#ifndef minor
#define minor(dev) ((int)((dev) & 0xFF))
#endif

#ifndef makedev
#define makedev(ma, mi) ((dev_t)((((dev_t)(ma)) << 8) | ((dev_t)(mi) & 0xFF)))
#endif

#endif
