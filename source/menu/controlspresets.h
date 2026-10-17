/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlspresets.h
 *
 * The fixed data every ControlsModel shares: the presets, what DOS can see
 * (the emulated modes), and which controls the mapper has a source for. The
 * sample model and the mapper-backed model both read it from here, so a preset
 * or a mode is defined once.
 ***************************************************************************/
#ifndef _CONTROLSPRESETS_H_
#define _CONTROLSPRESETS_H_

#include "controlsmodel.h"

// ---- the mapper's pad sources (CPadBindGroup in sdl_mapper.cpp)

/** Sources in a pad group: the 12 buttons, then the D-pad up, right, down, left. */
enum { PAD_SOURCE_COUNT = 16 };

/**
 * The mapper pad source a control binds through, or -1 when it has none (stick movement, Capture, Home).
 * The order is the shim's joystick button order (SDL_input.cpp ReadPlayer): the pad group reads the
 * shim's SDL joystick by index, so the two must agree. A Wiimote's 1 and 2 share the sources of X and Y.
 */
int Pad_SourceOfControl(int control);
/** The control id for a pad source (0..PAD_SOURCE_COUNT-1), or -1. The inverse of Pad_SourceOfControl() for the controls that have one. */
int Pad_ControlOfSource(int source);

// ---- presets

enum { PRESET_COMPATIBLE, PRESET_ARROWS, PRESET_SHOOTER, PRESET_POINT, PRESET_MENU, PRESET_COUNT };

const std::vector<PresetInfo>& Presets_All();
/** What a preset gives a control (nothing for a control with no entry or no pad source). */
BindingTarget Presets_Target(int preset, int control);
/** Likely targets for a control: what each preset gives it that the player can use, without duplicates or Not assigned. */
std::vector<BindingTarget> Presets_Suggestions(const ControlsModel& model, int player, int control);

// ---- what DOS sees

/**
 * Joystick, Two joysticks, Keyboard + mouse, then the advanced types (4axis_2, fcs, ch). The first three are
 * the Emulating list; see EmulatedMode::advanced.
 */
const std::vector<EmulatedMode>& Emulated_Modes();
/** The mode for a joystick.joysticktype value (auto is 4axis), or -1 for a value none of them stands for. */
int Emulated_IndexOfType(const std::string& type);

#endif
