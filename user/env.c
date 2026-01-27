#include <stddef.h>

// Minimal default environment - login sets proper values for USER/HOME/SHELL
// These are just fallbacks for processes that don't go through login
static char env_term[] = "TERM=xterm-256color";
static char env_path[] = "PATH=/bin:/usr/bin";

static char* default_envp[] = {
    env_term,
    env_path,
    NULL,
};

char** environ = default_envp;

// Some ports (and some libc variants) reference these names.
extern char** __environ __attribute__((weak, alias("environ")));
extern char** _environ __attribute__((weak, alias("environ")));
