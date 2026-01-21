#ifndef TASK_H
#define TASK_H

#include "interrupts.h"
#include "types.h"

void tasking_init(void);
bool tasking_is_enabled(void);

// Called from the IRQ0 (timer) path to perform a timeslice switch.
interrupt_frame_t* tasking_on_timer_tick(interrupt_frame_t* frame);

// Voluntary context switches (used by syscalls).
interrupt_frame_t* tasking_yield(interrupt_frame_t* frame);
interrupt_frame_t* tasking_exit(interrupt_frame_t* frame, int32_t exit_code);

// Create a user-mode task that starts at `entry` with user stack pointer `user_esp`.
bool tasking_spawn_user(uint32_t entry, uint32_t user_esp, uint32_t* page_directory, uint32_t user_brk);

// Create a user-mode task and return its pid (0 on failure).
uint32_t tasking_spawn_user_pid(uint32_t entry, uint32_t user_esp, uint32_t* page_directory, uint32_t user_brk);

typedef enum {
    TASK_STATE_RUNNABLE = 0,
    TASK_STATE_SLEEPING = 1,
    TASK_STATE_WAITING  = 2,
    TASK_STATE_ZOMBIE   = 3,
} task_state_t;

typedef struct task_info {
    uint32_t pid;
    bool user;
    task_state_t state;
    uint32_t cpu_ticks;
    uint32_t eip;
    uint32_t esp;
    int32_t exit_code;
    uint32_t wake_tick;
    uint32_t wait_pid;
    char name[16];
} task_info_t;

uint32_t tasking_current_pid(void);
uint32_t tasking_task_count(void);
bool tasking_get_task_info(uint32_t index, task_info_t* out);

// Put the current task to sleep until `wake_tick` (timer_get_ticks() units).
interrupt_frame_t* tasking_sleep_until(interrupt_frame_t* frame, uint32_t wake_tick);

// Wait for a task to exit (returns exit code in EAX via the syscall frame).
interrupt_frame_t* tasking_wait(interrupt_frame_t* frame, uint32_t pid);

// Kill a task by pid. Returns 0 on success, or -errno.
int32_t tasking_kill(uint32_t pid, int32_t exit_code);

// Adjust the user heap break (sbrk-style). Returns previous brk in EAX or -1.
interrupt_frame_t* tasking_sbrk(interrupt_frame_t* frame, int32_t increment);

// Minimal fd-based API for syscalls (per-task).
int32_t tasking_fd_open(const char* path, uint32_t flags);
int32_t tasking_fd_close(int32_t fd);
int32_t tasking_fd_read(int32_t fd, void* dst_user, uint32_t len);
int32_t tasking_fd_write(int32_t fd, const void* src_user, uint32_t len);
int32_t tasking_fd_lseek(int32_t fd, int32_t offset, int32_t whence);
int32_t tasking_fd_fstat(int32_t fd, void* st_user);
int32_t tasking_stat(const char* path, void* st_user);
int32_t tasking_mkdir(const char* path);
int32_t tasking_readdir(int32_t fd, void* dirent_user);
int32_t tasking_chdir(const char* path);
int32_t tasking_getcwd(void* dst_user, uint32_t len);
int32_t tasking_fd_ioctl(int32_t fd, uint32_t req, void* argp_user);
int32_t tasking_unlink(const char* path);
int32_t tasking_rename(const char* old_path, const char* new_path);
int32_t tasking_rmdir(const char* path);
int32_t tasking_truncate(const char* path, uint32_t new_size);
int32_t tasking_fd_ftruncate(int32_t fd, uint32_t new_size);
int32_t tasking_fd_fsync(int32_t fd);
int32_t tasking_fd_dup(int32_t oldfd);
int32_t tasking_fd_dup2(int32_t oldfd, int32_t newfd);
int32_t tasking_pipe(void* pipefds_user);

// Spawn a new user process by loading an ELF from the VFS, inheriting the
// caller's cwd/tty settings. Returns pid (>0) on success or -errno.
int32_t tasking_spawn_exec(const char* path, const char* const* argv, uint32_t argc);

#endif
