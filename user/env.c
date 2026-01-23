#include <stddef.h>

static char env_term[] = "TERM=xterm-256color";
static char env_path[] = "PATH=/bin:/usr/bin";
static char env_home[] = "HOME=/home/victor";
static char env_user[] = "USER=victor";
static char env_logname[] = "LOGNAME=victor";
static char env_shell[] = "SHELL=/bin/sh";

static char* default_envp[] = {
    env_term,
    env_path,
    env_home,
    env_user,
    env_logname,
    env_shell,
    NULL,
};

char** environ = default_envp;

// Some ports (and some libc variants) reference these names.
extern char** __environ __attribute__((weak, alias("environ")));
extern char** _environ __attribute__((weak, alias("environ")));
