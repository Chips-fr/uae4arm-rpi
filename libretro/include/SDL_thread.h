//FIXME RETRO HACK REDONE ALL THIS CRAPPY
//SDLTHREAD
#ifndef SDL_THREAD_H
#define SDL_THREAD_H 1

// only because we need 
/*
SDL_sem
SDL_Thread
SDL_SemWait
SDL_CreateSemaphore
SDL_CreateThread
SDL_KillThread
SDL_DestroySemaphore
SDL_SemPost
*/
//in rs232.c

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern long GetTicks(void);
#include <stdint.h>

#define Uint32 uint32_t
#define SDL_GetTicks  GetTicks 
#define SDL_Delay(a) usleep((a)*1000)

#define SDL_KillThread(X)

#define ERR_MAX_STRLEN	128
#define ERR_MAX_ARGS	5
#define SDLCALL
#define SDL_zero(x)	memset(&(x), 0, sizeof((x)))
#define SDL_malloc	malloc
#define SDL_MUTEX_TIMEDOUT	1
#define SDL_MUTEX_MAXWAIT	(~(Uint32)0)
#define SDL_SetError printf
#define SDL_free free

typedef struct SDL_error
{    
    int error;
    char key[ERR_MAX_STRLEN];
    int argc;
    union
    {
        void *value_ptr;
        int value_i;
        double value_f;
        char buf[ERR_MAX_STRLEN];
    } args[ERR_MAX_ARGS];
} SDL_error;


#include <sys/errno.h> 
#include <unistd.h>
#include <signal.h>

#include <pthread.h>
#include <semaphore.h>

struct SDL_semaphore
{
    sem_t sem;
};

struct SDL_semaphore;
typedef struct SDL_semaphore SDL_sem;

/* Create a counting semaphore */
extern SDL_sem *
SDL_CreateSemaphore(Uint32 initial_value);


extern void SDL_DestroySemaphore(SDL_sem *sem);

extern int SDL_SemTryWait(SDL_sem *sem);

extern int SDL_SemWait(SDL_sem *sem);

extern int SDL_SemWaitTimeout(SDL_sem *sem, Uint32 timeout);

extern Uint32 SDL_SemValue(SDL_sem *sem);

extern int SDL_SemPost(SDL_sem *sem);

typedef struct SDL_Thread SDL_Thread;
typedef unsigned long SDL_threadID;

/* The SDL thread priority
 *
 * Note: On many systems you require special privileges to set high priority.
 */
typedef enum {
    SDL_THREAD_PRIORITY_LOW,
    SDL_THREAD_PRIORITY_NORMAL,
    SDL_THREAD_PRIORITY_HIGH
} SDL_ThreadPriority;

typedef int (SDLCALL * SDL_ThreadFunction) (void *data);
//typedef sys_ppu_thread_t SYS_ThreadHandle;
typedef pthread_t SYS_ThreadHandle;

/* This is the system-independent thread info structure */
struct SDL_Thread
{
    SDL_threadID threadid;
    SYS_ThreadHandle handle;
    int status;
    SDL_error errbuf;
    void *data;
};

struct SDL_mutex
{
    int recursive;
    SDL_threadID owner;
    SDL_sem *sem;
};

typedef struct SDL_mutex SDL_mutex;
extern SDL_mutex *SDL_CreateMutex(void);


/* Create a mutex */
extern SDL_mutex *
SDL_CreateMutex(void);

/* Free the mutex */
extern void
SDL_DestroyMutex(SDL_mutex * mutex);

/* WARNING:  This may not work for systems with 64-bit pid_t */
extern Uint32 SDL_ThreadID(void);

/* Lock the semaphore */
extern int
SDL_mutexP(SDL_mutex * mutex);

/* Unlock the mutex */
extern int
SDL_mutexV(SDL_mutex * mutex);

#define ARRAY_CHUNKSIZE	32
/* The array of threads currently active in the application
   (except the main thread)
   The manipulation of an array here is safer than using a linked list.
*/
extern void
SDL_MaskSignals(sigset_t * omask);
extern void
SDL_UnmaskSignals(sigset_t * omask);
/* Arguments and callback to setup and run the user thread function */
typedef struct
{
    int (SDLCALL * func) (void *);
    void *data;
    SDL_Thread *info;
    SDL_sem *wait;
} thread_args;

extern void
SDL_SYS_SetupThread(void);

extern void
SDL_RunThread(void *data);

extern int SDL_SYS_CreateThread(SDL_Thread *thread, void *args);

extern void SDL_SYS_WaitThread(SDL_Thread *thread);

#define DECLSPEC

#ifdef SDL_PASSED_BEGINTHREAD_ENDTHREAD
#undef SDL_CreateThread
extern DECLSPEC SDL_Thread *SDLCALL
SDL_CreateThread(int (SDLCALL * fn) (void *), void *data,
                 pfnSDL_CurrentBeginThread pfnBeginThread,
                 pfnSDL_CurrentEndThread pfnEndThread)
#else
extern DECLSPEC SDL_Thread *SDLCALL
SDL_CreateThread(int (SDLCALL * fn) (void *), void *data)
#endif
;

extern SDL_threadID
SDL_GetThreadID(SDL_Thread * thread);

extern int
SDL_SetThreadPriority(SDL_ThreadPriority priority);

extern void
SDL_WaitThread(SDL_Thread * thread, int *status);


#endif

