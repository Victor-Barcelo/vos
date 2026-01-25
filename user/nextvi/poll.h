/* Minimal poll.h stub for VOS */
#ifndef _POLL_H
#define _POLL_H

#define POLLIN   0x001
#define POLLOUT  0x004
#define POLLERR  0x008
#define POLLHUP  0x010
#define POLLNVAL 0x020

struct pollfd {
    int fd;
    short events;
    short revents;
};

/* Simple poll implementation - just check if we can read */
static inline int poll(struct pollfd *fds, int nfds, int timeout) {
    (void)timeout;
    for (int i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].fd >= 0) {
            if (fds[i].events & POLLIN)
                fds[i].revents |= POLLIN;  /* assume readable */
            if (fds[i].events & POLLOUT)
                fds[i].revents |= POLLOUT; /* assume writable */
        }
    }
    return nfds;
}

#endif /* _POLL_H */
