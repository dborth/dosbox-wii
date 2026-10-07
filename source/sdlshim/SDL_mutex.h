/****************************************************************************
 * DOSBox Wii
 * SDL_mutex.h
 *
 * SDL 1.2's mutex API, implemented on the platform HAL (Mutex).
 *
 * One difference from SDL: SDL's mutexes are recursive, the HAL's are not.
 * The only user in the core, the CD image player (cdrom_image.cpp), never
 * locks one it already holds. SDL_cond is declared only because
 * SDL_Block (input.cpp) has a pointer to one; there is no condition
 * variable or semaphore API, and the core calls none.
 *
 * Deliberately includes nothing: it is used by DOSBox core files and
 * platform files whose headers collide.
 ***************************************************************************/
#ifndef SDL_MUTEX_H
#define SDL_MUTEX_H

typedef struct SDL_mutex SDL_mutex;
typedef struct SDL_cond SDL_cond;

//! Creates an unlocked mutex. Returns NULL on failure.
SDL_mutex * SDL_CreateMutex(void);

//! Destroys a mutex that is not locked. NULL is ignored.
void SDL_DestroyMutex(SDL_mutex * mutex);

//! Locks the mutex, waiting for it if another thread holds it. Returns 0,
//! or -1 if mutex is NULL.
int SDL_mutexP(SDL_mutex * mutex);

//! Unlocks the mutex. Returns 0, or -1 if mutex is NULL.
int SDL_mutexV(SDL_mutex * mutex);

#define SDL_LockMutex(m)   SDL_mutexP(m)
#define SDL_UnlockMutex(m) SDL_mutexV(m)

#endif
