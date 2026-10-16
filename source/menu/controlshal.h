/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlshal.h
 *
 * The console behind the controller screens: the HAL's controllers, keyboard
 * and mouse, read through the ControlsHardware seam (controlsmodel.h). Needs
 * the platform, so the host tests use the sample hardware instead.
 ***************************************************************************/
#ifndef _CONTROLSHAL_H_
#define _CONTROLSHAL_H_

#include "controlsmodel.h"

/** The HAL-backed hardware. Install it once the platform is up: Controls_SetHardware(&Controls_Hal()). */
ControlsHardware& Controls_Hal();

#endif
