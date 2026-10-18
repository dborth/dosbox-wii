/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * samplecontrols.h
 *
 * Sample data for the host tests and previews (see samplecontrols.cpp): four
 * invented players and a model over them, with hooks to change what they
 * hold and press. Call SampleControls_Use() once at the start of a test.
 ***************************************************************************/
#ifndef _SAMPLECONTROLS_H_
#define _SAMPLECONTROLS_H_

#include "controlsmodel.h"

/** Makes the sample model what Controls() returns when no model is installed (Controls_SetModel(nullptr) goes back to it). */
void SampleControls_Use();

/** Installs where the sample model reads the console from; null goes back to the sample players. Must outlive its use. */
void Controls_SetHardware(ControlsHardware * hardware);

/** Sets what the sample hardware reports as held (does nothing once another model is installed). */
void SampleControls_SetHeld(uint32_t bits);
/** Queues a press for the sample model's pollCapture(). */
void SampleControls_PushCapture(const CaptureEvent& event);
/** Makes a sample player a different kind of hardware (PadType::NONE: nothing connected). */
void SampleControls_SetHardware(int player, PadType type, bool sideways = false, bool stickClicks = false);
/** Puts the sample players back to their usual hardware. */
void SampleControls_ResetHardware();
/** Makes a sample player hold several devices on one channel (a Wiimote and a GameCube pad), primary first; the first sets the type. */
void SampleControls_SetDevices(int player, const std::vector<PadType>& devices, bool sideways = false, bool stickClicks = false);

#endif
