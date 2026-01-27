#ifndef SDL_MUTEX_H
#define SDL_MUTEX_H

#include "SDL_stdinc.h"

// Mutex (no-op for VOS - single threaded)
typedef struct SDL_mutex {
    int dummy;
} SDL_mutex;

SDL_mutex* SDL_CreateMutex(void);
void SDL_DestroyMutex(SDL_mutex *mutex);
int SDL_LockMutex(SDL_mutex *mutex);
int SDL_UnlockMutex(SDL_mutex *mutex);
int SDL_TryLockMutex(SDL_mutex *mutex);

// Semaphore stubs
typedef struct SDL_sem {
    int value;
} SDL_sem;

SDL_sem* SDL_CreateSemaphore(Uint32 initial_value);
void SDL_DestroySemaphore(SDL_sem *sem);
int SDL_SemWait(SDL_sem *sem);
int SDL_SemTryWait(SDL_sem *sem);
int SDL_SemPost(SDL_sem *sem);
Uint32 SDL_SemValue(SDL_sem *sem);

// Condition variable stubs
typedef struct SDL_cond {
    int dummy;
} SDL_cond;

SDL_cond* SDL_CreateCond(void);
void SDL_DestroyCond(SDL_cond *cond);
int SDL_CondSignal(SDL_cond *cond);
int SDL_CondBroadcast(SDL_cond *cond);
int SDL_CondWait(SDL_cond *cond, SDL_mutex *mutex);

// Thread stubs (VOS has no threads)
typedef struct SDL_Thread SDL_Thread;
typedef int (*SDL_ThreadFunction)(void *data);

SDL_Thread* SDL_CreateThread(SDL_ThreadFunction fn, const char *name, void *data);
void SDL_WaitThread(SDL_Thread *thread, int *status);
Uint32 SDL_GetThreadID(SDL_Thread *thread);
Uint32 SDL_ThreadID(void);

#define SDL_MUTEX_TIMEDOUT 1

#endif
