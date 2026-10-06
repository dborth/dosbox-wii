/****************************************************************************
 * DOSBox Wii
 * SDL_timer.h
 *
 * SDL 1.2's tick counter and delay, implemented on the platform HAL
 * (SystemTime, ThreadDriver). SDL_AddTimer() and friends are not provided:
 * the core does not use them.
 *
 * Deliberately includes only <stdint.h>: it is used by DOSBox core files
 * and platform files whose headers collide.
 ***************************************************************************/
#ifndef SDL_TIMER_H
#define SDL_TIMER_H

#include <stdint.h>

/* Same type as in SDL_input.h; repeating an identical typedef is legal. */
typedef uint32_t Uint32;

//! Milliseconds since program start. Wraps at 2^32 ms (49.7 days), as SDL's
//! did; unsigned differences stay correct across the wrap. Any thread.
Uint32 SDL_GetTicks(void);

//! Sleeps the calling thread for at least `ms` milliseconds.
void SDL_Delay(Uint32 ms);

#endif
