#ifndef VOS_SYS_UTSNAME_H
#define VOS_SYS_UTSNAME_H

// Minimal uname()/utsname support for ports like sbase.
// newlib's bare-metal headers don't ship <sys/utsname.h>.

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SYS_NMLN
#define SYS_NMLN 65
#endif

struct utsname {
    char sysname[SYS_NMLN];
    char nodename[SYS_NMLN];
    char release[SYS_NMLN];
    char version[SYS_NMLN];
    char machine[SYS_NMLN];
};

int uname(struct utsname* buf);

#ifdef __cplusplus
}
#endif

#endif

