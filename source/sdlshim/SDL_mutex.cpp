/****************************************************************************
 * DOSBox Wii
 * SDL_mutex.cpp
 *
 * See SDL_mutex.h.
 ***************************************************************************/
#include <new>

#include "SDL_mutex.h"

#include "drivers/Mutex.h"

struct SDL_mutex
{
	Mutex mutex;
};

SDL_mutex * SDL_CreateMutex(void)
{
	return new(std::nothrow) SDL_mutex;
}

void SDL_DestroyMutex(SDL_mutex * mutex)
{
	delete mutex;
}

int SDL_mutexP(SDL_mutex * mutex)
{
	if (!mutex)
		return -1;
	mutex->mutex.lock();
	return 0;
}

int SDL_mutexV(SDL_mutex * mutex)
{
	if (!mutex)
		return -1;
	mutex->mutex.unlock();
	return 0;
}
