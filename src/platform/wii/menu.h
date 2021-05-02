#ifndef _MENUH_
#define _MENUH_

extern int MENU_CyclesDisplay;
extern int MENU_FrameskipDisplay;


/**
 * Increase or decrease the current CPU cycles.
 *
 * Implemented in src/cpu/cpu.cpp
 *
 * @param increase True for increase cycles. False for decrease cycles.
 */
void MENU_CycleIncreaseOrDecrease(bool increase);

/**
 * Increase or decrease the current frameskip
 *
 * Implemented in src/gui/render.cpp
 *
 * @param increase True for increase cycles. False for decrease cycles.
 */
void MENU_IncreaseOrDecreaseFrameSkip(bool increase);

#endif
