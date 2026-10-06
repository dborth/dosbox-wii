/****************************************************************************
 * DOSBox Wii
 * SDL.h
 *
 * Stand-in for the SDL 1.2 umbrella header. No SDL library is linked: the
 * core keeps its `#include "SDL.h"` lines, and each part of SDL's API that
 * DOSBox still uses is declared in a header here and implemented on the
 * platform HAL.
 *
 *   SDL_input.h   events, keyboard, mouse, joystick   (SDL_input.cpp)
 *   SDL_timer.h   SDL_GetTicks, SDL_Delay             (SDL_timer.cpp)
 *   SDL_mutex.h   SDL_mutex (SDL_thread.h includes it) (SDL_mutex.cpp)
 *   SDL_cdrom.h   SDL_CD*, no drives (stub)           (SDL_cdrom.cpp)
 *
 * Platform files whose headers collide with the core's should include the
 * individual SDL_*.h they need, not this file.
 ***************************************************************************/
#ifndef SDL_H
#define SDL_H

#include "SDL_input.h"
#include "SDL_timer.h"
#include "SDL_mutex.h"
#include "SDL_cdrom.h"

#endif
