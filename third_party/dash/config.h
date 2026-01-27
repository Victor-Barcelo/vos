/* config.h for VOS dash port */

#ifndef DASH_CONFIG_H
#define DASH_CONFIG_H

/* Package info */
#define PACKAGE "dash"
#define PACKAGE_BUGREPORT ""
#define PACKAGE_NAME "dash"
#define PACKAGE_STRING "dash 0.5.12"
#define PACKAGE_TARNAME "dash"
#define PACKAGE_URL ""
#define PACKAGE_VERSION "0.5.12"
#define VERSION "0.5.12"

/* Standard headers */
#define STDC_HEADERS 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_ALLOCA_H 1
#define HAVE_PATHS_H 1

/* Functions available in VOS/newlib */
#define HAVE_FNMATCH 1
#define HAVE_BSEARCH 1
#define HAVE_GETPWNAM 1
#define HAVE_ISALPHA 1
#define HAVE_KILLPG 1
#undef HAVE_MEMPCPY
#define HAVE_STPCPY 1
#undef HAVE_STRCHRNUL
#define HAVE_STRTOD 1
#define HAVE_STRTOIMAX 1
#define HAVE_STRTOUMAX 1
#define HAVE_SYSCONF 1
#define HAVE_DECL_ISBLANK 1

/* Functions NOT available - use dash builtins */
#undef HAVE_GLOB
#undef HAVE_GETRLIMIT  /* ulimit won't work fully */
#define HAVE_STRSIGNAL 1

/* stat64 - VOS uses 32-bit */
#undef HAVE_STAT64

/* Signal handling */
#undef HAVE_SIGSETMASK
#define signal signal

/* Job control - enable it, we have the syscalls */
#define JOBS 1

/* BSD compatibility - enables sys/wait.h and related includes */
#define BSD 1

/* Use small/minimal build - disables history editing (libedit) */
#define SMALL 1

/* Line number tracking in scripts */
#define WITH_LINENO 1

/* No libedit - disable history editing */
#undef WITH_LIBEDIT

/* Disable mempcpy since newlib may not have it */
#undef HAVE_MEMPCPY

/* Path definitions */
#define _PATH_BSHELL "/bin/sh"
#define _PATH_DEVNULL "/dev/null"
#define _PATH_TTY "/dev/tty"

/* Integer sizes for arithmetic */
#define SIZEOF_INTMAX_T 4
#define SIZEOF_LONG_LONG_INT 8
#define PRIdMAX "ld"

/* Use standard 32-bit versions, not 64-bit variants */
/* These macros map the 64-bit names to standard names */
#define dirent64 dirent
#define readdir64 readdir
#define glob64 glob
#define glob64_t glob_t
#define globfree64 globfree
#define stat64 stat
#define fstat64 fstat
#define lstat64 lstat
#define open64 open

/* GCC attributes */
#define HAVE_ALIAS_ATTRIBUTE 1

#endif /* DASH_CONFIG_H */
