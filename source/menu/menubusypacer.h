/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * menubusypacer.h
 *
 * When the "please wait" panel (menubusy.h) comes up and goes, and how fast its
 * spinner turns, as a function of time alone. Header only, no GUI, so a host
 * test can run it.
 ***************************************************************************/
#ifndef _MENUBUSYPACER_H_
#define _MENUBUSYPACER_H_

#include <stdint.h>

/**
 * Work that finishes quickly shows nothing: the panel waits DELAY_MS before it comes up, so a save that takes a
 * moment does not flash it. Once up it stays at least MIN_SHOWN_MS, so it can be read and is not a flicker. The spinner
 * turns one dot every STEP_MS from the moment it is shown.
 *
 * Call update() every frame with the milliseconds since the work began and whether it has finished; read shown(),
 * phase() and over() after it.
 */
class MenuBusyPacer
{
	public:
		enum { DELAY_MS = 400, MIN_SHOWN_MS = 350, STEP_MS = 90 };

		MenuBusyPacer() : isShown(false), shownAt(0), spin(0), finished(false) {}

		void update(uint32_t elapsedMs, bool done)
		{
			// work that is finished by the time the panel was due is not worth showing it for
			if(!isShown && !done && elapsedMs >= DELAY_MS)
			{
				isShown = true;
				shownAt = elapsedMs;
			}
			spin = isShown ? (int)((elapsedMs - shownAt) / STEP_MS) : 0;
			finished = done && (!isShown || elapsedMs - shownAt >= MIN_SHOWN_MS);
		}

		/** Whether the panel is drawn. */
		bool shown() const { return isShown; }
		/** The spinner's step: grows by one every STEP_MS while shown. */
		int phase() const { return spin; }
		/** Whether the wait is over: the work is done and the panel (if it was shown) has stayed long enough. */
		bool over() const { return finished; }

	private:
		bool isShown;
		uint32_t shownAt;
		int spin;
		bool finished;
};

#endif
