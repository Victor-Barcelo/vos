/*
 * poll.h - VOS poll system call definitions
 */

#ifndef _SYS_POLL_H
#define _SYS_POLL_H

#include "../syscall.h"

/* Poll events - map to VOS definitions */
#define POLLIN     VOS_POLLIN
#define POLLOUT    VOS_POLLOUT
#define POLLERR    0x0008
#define POLLHUP    0x0010
#define POLLNVAL   0x0020
#define POLLPRI    0x0002

/* pollfd structure - use VOS definition */
struct pollfd {
    int fd;
    short events;
    short revents;
};

typedef unsigned int nfds_t;

/* poll function - wraps sys_poll */
static inline int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    /* Convert from struct pollfd to vos_pollfd_t */
    vos_pollfd_t vos_fds[64];  /* Max 64 fds */
    if (nfds > 64) nfds = 64;

    for (nfds_t i = 0; i < nfds; i++) {
        vos_fds[i].fd = fds[i].fd;
        vos_fds[i].events = (uint16_t)fds[i].events;
        vos_fds[i].revents = 0;
    }

    int ret = sys_poll(vos_fds, (uint32_t)nfds, timeout);

    /* Copy back revents */
    for (nfds_t i = 0; i < nfds; i++) {
        fds[i].revents = (short)vos_fds[i].revents;
    }

    return ret;
}

#endif /* _SYS_POLL_H */
