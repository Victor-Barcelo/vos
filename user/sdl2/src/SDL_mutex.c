/*
 * SDL_mutex.c - VOS SDL2 mutex/thread stubs
 *
 * VOS is single-threaded, so these are no-op stubs.
 * They exist to allow SDL2 applications that use mutexes
 * to compile and run (without actual thread safety).
 */

#include "SDL2/SDL_mutex.h"
#include <stdlib.h>

/* Mutex functions - no-op since VOS is single-threaded */

SDL_mutex* SDL_CreateMutex(void) {
    SDL_mutex *mutex = (SDL_mutex*)malloc(sizeof(SDL_mutex));
    if (mutex) {
        mutex->dummy = 0;
    }
    return mutex;
}

void SDL_DestroyMutex(SDL_mutex *mutex) {
    if (mutex) {
        free(mutex);
    }
}

int SDL_LockMutex(SDL_mutex *mutex) {
    (void)mutex;
    return 0;  /* Success */
}

int SDL_UnlockMutex(SDL_mutex *mutex) {
    (void)mutex;
    return 0;  /* Success */
}

int SDL_TryLockMutex(SDL_mutex *mutex) {
    (void)mutex;
    return 0;  /* Success - always acquired since no contention */
}

/* Semaphore functions - maintain counter for compatibility */

SDL_sem* SDL_CreateSemaphore(Uint32 initial_value) {
    SDL_sem *sem = (SDL_sem*)malloc(sizeof(SDL_sem));
    if (sem) {
        sem->value = (int)initial_value;
    }
    return sem;
}

void SDL_DestroySemaphore(SDL_sem *sem) {
    if (sem) {
        free(sem);
    }
}

int SDL_SemWait(SDL_sem *sem) {
    if (sem && sem->value > 0) {
        sem->value--;
        return 0;  /* Success */
    }
    /* In a real threaded environment, this would block.
     * Since VOS is single-threaded, we just return success
     * to avoid deadlock. */
    if (sem) {
        sem->value--;
    }
    return 0;
}

int SDL_SemTryWait(SDL_sem *sem) {
    if (sem && sem->value > 0) {
        sem->value--;
        return 0;  /* Success */
    }
    return SDL_MUTEX_TIMEDOUT;  /* Would block */
}

int SDL_SemPost(SDL_sem *sem) {
    if (sem) {
        sem->value++;
    }
    return 0;  /* Success */
}

Uint32 SDL_SemValue(SDL_sem *sem) {
    if (sem) {
        return (Uint32)(sem->value > 0 ? sem->value : 0);
    }
    return 0;
}

/* Condition variable functions - no-op stubs */

SDL_cond* SDL_CreateCond(void) {
    SDL_cond *cond = (SDL_cond*)malloc(sizeof(SDL_cond));
    if (cond) {
        cond->dummy = 0;
    }
    return cond;
}

void SDL_DestroyCond(SDL_cond *cond) {
    if (cond) {
        free(cond);
    }
}

int SDL_CondSignal(SDL_cond *cond) {
    (void)cond;
    return 0;  /* Success */
}

int SDL_CondBroadcast(SDL_cond *cond) {
    (void)cond;
    return 0;  /* Success */
}

int SDL_CondWait(SDL_cond *cond, SDL_mutex *mutex) {
    (void)cond;
    (void)mutex;
    /* In a real threaded environment, this would block.
     * Since VOS is single-threaded, we just return success. */
    return 0;
}

/* Thread functions - VOS has no threads, return NULL/0 */

SDL_Thread* SDL_CreateThread(SDL_ThreadFunction fn, const char *name, void *data) {
    (void)fn;
    (void)name;
    (void)data;
    /* Cannot create threads in VOS - return NULL to indicate failure */
    return NULL;
}

void SDL_WaitThread(SDL_Thread *thread, int *status) {
    (void)thread;
    if (status) {
        *status = 0;
    }
}

Uint32 SDL_GetThreadID(SDL_Thread *thread) {
    (void)thread;
    return 0;
}

Uint32 SDL_ThreadID(void) {
    /* Return a constant "main thread" ID */
    return 1;
}
