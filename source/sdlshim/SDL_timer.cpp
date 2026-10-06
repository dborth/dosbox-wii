/****************************************************************************
 * DOSBox Wii
 * SDL_timer.cpp
 *
 * See SDL_timer.h.
 ***************************************************************************/
#include "SDL_timer.h"

#include "drivers/Platform.h"
#include "drivers/ThreadDriver.h"
#include "drivers/Time.h"

// Sampled during static initialisation, before main() and before any other
// thread exists, so no locking is needed. Counting from here rather than
// from console power-on keeps the 2^32 ms wrap 49.7 days away from the start
// of the session: increaseticks() in dosbox.cpp compares tick values
// directly and would misbehave at a wrap.
static const Ticks startTicks = SystemTime::now();

Uint32 SDL_GetTicks(void)
{
	return (Uint32)SystemTime::diffMillisecs(startTicks, SystemTime::now());
}

void SDL_Delay(Uint32 ms)
{
	platform->getThread()->sleepMilliseconds(ms);
}
