# Chapter 25: Shell and Commands

VOS provides both a kernel-mode shell for debugging and a user-mode shell for normal operation.

## Kernel Shell

The kernel shell runs in ring 0 and provides basic system interaction during development.

### Built-in Commands

| Command | Description |
|---------|-------------|
| help | Display available commands |
| clear, cls | Clear the screen |
| echo <text> | Print text |
| info, about | Show system information |
| uptime | Show system uptime |
| date | Show current date/time |
| setdate | Set RTC date/time |
| ls [path] | List directory contents |
| cat <file> | Display file contents |
| run <program> | Execute ELF program |
| ps | Show running processes |
| kill <pid> | Send signal to process |
| free | Show memory usage |
| color <n> | Change text color |
| basic | Start BASIC interpreter |
| reboot | Reboot the system |
| halt | Halt the system |

### Shell Implementation

```c
void shell_run(void) {
    char command_buffer[256];

    statusbar_init();
    keyboard_set_idle_hook(shell_idle_hook);

    screen_println("Welcome to VOS Shell!");
    screen_println("Type 'help' for available commands.\n");

    while (1) {
        print_prompt();
        keyboard_getline(command_buffer, 256);
        keyboard_history_add(command_buffer);
        execute_command(command_buffer);
    }
}
```

## User Shell (dash)

VOS uses **dash** (Debian Almquist Shell) as the default user shell. It runs in ring 3 with full process support. Dash is a POSIX-compliant shell that is fast, lightweight, and well-tested.

Both `/bin/sh` and `/bin/dash` point to dash.

### Features

- POSIX-compliant shell scripting
- Command execution with PATH search
- Argument parsing and word expansion
- Environment variables
- Pipes (`|`)
- Redirection (`>`, `>>`, `<`, `2>&1`)
- Background jobs (`&`)
- Signal handling (Ctrl+C, Ctrl+Z)
- Job control (fg, bg, jobs)
- Shell builtins (cd, export, read, etc.)

### Built-in Commands

| Command | Description |
|---------|-------------|
| cd <dir> | Change directory |
| pwd | Print working directory |
| export VAR=value | Set environment variable |
| unset VAR | Remove environment variable |
| echo | Print arguments |
| exit | Exit shell |
| jobs | List background jobs |
| fg [n] | Bring job to foreground |
| bg [n] | Continue job in background |
| wait [pid] | Wait for process |

### Command Execution

```c
void execute_command(char *line) {
    // Parse command line
    command_t cmd;
    parse_command(line, &cmd);

    // Handle built-ins
    if (is_builtin(cmd.argv[0])) {
        execute_builtin(&cmd);
        return;
    }

    // Fork and exec
    pid_t pid = fork();
    if (pid == 0) {
        // Child
        setup_redirections(&cmd);
        execvp(cmd.argv[0], cmd.argv);
        fprintf(stderr, "%s: command not found\n", cmd.argv[0]);
        exit(127);
    }

    // Parent
    if (!cmd.background) {
        // Wait for foreground process
        int status;
        waitpid(pid, &status, 0);
    } else {
        // Add to job list
        add_job(pid, line);
        printf("[%d] %d\n", job_count, pid);
    }
}
```

### Pipes

```c
void execute_pipeline(command_t *cmds, int count) {
    int prev_pipe[2] = {-1, -1};

    for (int i = 0; i < count; i++) {
        int next_pipe[2];
        if (i < count - 1) {
            pipe(next_pipe);
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Set up stdin from previous pipe
            if (prev_pipe[0] != -1) {
                dup2(prev_pipe[0], STDIN_FILENO);
                close(prev_pipe[0]);
                close(prev_pipe[1]);
            }

            // Set up stdout to next pipe
            if (i < count - 1) {
                dup2(next_pipe[1], STDOUT_FILENO);
                close(next_pipe[0]);
                close(next_pipe[1]);
            }

            execvp(cmds[i].argv[0], cmds[i].argv);
            exit(127);
        }

        // Parent: close used pipe ends
        if (prev_pipe[0] != -1) {
            close(prev_pipe[0]);
            close(prev_pipe[1]);
        }

        prev_pipe[0] = next_pipe[0];
        prev_pipe[1] = next_pipe[1];
    }

    // Wait for all children
    for (int i = 0; i < count; i++) {
        wait(NULL);
    }
}
```

### Redirection

```c
void setup_redirections(command_t *cmd) {
    if (cmd->input_file) {
        int fd = open(cmd->input_file, O_RDONLY);
        if (fd < 0) {
            perror(cmd->input_file);
            exit(1);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (cmd->output_file) {
        int flags = O_WRONLY | O_CREAT;
        flags |= cmd->append ? O_APPEND : O_TRUNC;
        int fd = open(cmd->output_file, flags, 0644);
        if (fd < 0) {
            perror(cmd->output_file);
            exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}
```

## External Utilities

VOS includes many sbase utilities:

### File Operations

| Command | Description |
|---------|-------------|
| cat | Concatenate files |
| cp | Copy files |
| mv | Move/rename files |
| rm | Remove files |
| mkdir | Create directory |
| rmdir | Remove directory |
| ls | List directory |
| ln | Create links |
| chmod | Change permissions |
| touch | Update timestamps |

### Text Processing

| Command | Description |
|---------|-------------|
| grep | Search patterns |
| sed | Stream editor |
| awk | Pattern processing |
| cut | Cut columns |
| sort | Sort lines |
| uniq | Remove duplicates |
| wc | Word count |
| head | Show first lines |
| tail | Show last lines |
| tr | Translate characters |

### System Utilities

| Command | Description |
|---------|-------------|
| ps | Process status |
| kill | Send signals |
| date | Show/set date |
| uname | System info |
| env | Show environment |
| which | Locate command |
| whoami | Current user |
| sleep | Delay |
| true/false | Return status |

### Development

| Command | Description |
|---------|-------------|
| tcc | Tiny C Compiler |
| make | Build tool |
| diff | Compare files |
| patch | Apply patches |

## Command Line Parsing

```c
typedef struct {
    char *argv[MAX_ARGS];
    int argc;
    char *input_file;
    char *output_file;
    bool append;
    bool background;
} command_t;

void parse_command(char *line, command_t *cmd) {
    memset(cmd, 0, sizeof(*cmd));

    char *token = strtok(line, " \t");
    while (token && cmd->argc < MAX_ARGS - 1) {
        if (strcmp(token, "<") == 0) {
            cmd->input_file = strtok(NULL, " \t");
        }
        else if (strcmp(token, ">") == 0) {
            cmd->output_file = strtok(NULL, " \t");
            cmd->append = false;
        }
        else if (strcmp(token, ">>") == 0) {
            cmd->output_file = strtok(NULL, " \t");
            cmd->append = true;
        }
        else if (strcmp(token, "&") == 0) {
            cmd->background = true;
        }
        else {
            cmd->argv[cmd->argc++] = token;
        }
        token = strtok(NULL, " \t");
    }
    cmd->argv[cmd->argc] = NULL;
}
```

## Signal Handling in Shell

```c
void setup_shell_signals(void) {
    // Shell ignores SIGINT/SIGTSTP in parent
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    // Handle SIGCHLD for background jobs
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
}

void sigchld_handler(int sig) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Update job status
        mark_job_done(pid, status);
    }
}

// Before exec in child
void setup_child_signals(void) {
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
}
```

## Job Control

```c
typedef struct job {
    int id;
    pid_t pid;
    char command[256];
    enum { JOB_RUNNING, JOB_STOPPED, JOB_DONE } state;
    struct job *next;
} job_t;

void builtin_fg(int job_id) {
    job_t *job = find_job(job_id);
    if (!job) {
        fprintf(stderr, "fg: no such job\n");
        return;
    }

    // Make foreground process group
    tcsetpgrp(STDIN_FILENO, job->pid);

    // Continue if stopped
    if (job->state == JOB_STOPPED) {
        kill(-job->pid, SIGCONT);
    }

    // Wait for it
    int status;
    waitpid(job->pid, &status, WUNTRACED);

    // Restore shell as foreground
    tcsetpgrp(STDIN_FILENO, getpgrp());

    if (WIFSTOPPED(status)) {
        job->state = JOB_STOPPED;
        printf("\n[%d]+ Stopped    %s\n", job->id, job->command);
    } else {
        remove_job(job);
    }
}
```

## Summary

VOS shell capabilities:

1. **Kernel shell** for basic system interaction
2. **User shell** with full POSIX features
3. **Built-in commands** for common operations
4. **External utilities** from sbase
5. **Pipes and redirection** for data flow
6. **Job control** with fg/bg/jobs
7. **Signal handling** for interactive use

---

*Previous: [Chapter 24: POSIX Overview and VOS Compliance](24_posix.md)*
*Next: [Chapter 26: Newlib Integration](26_newlib.md)*
