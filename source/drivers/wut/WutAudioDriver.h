/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutAudioDriver.h
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include <sndcore2/core.h>
#include <sndcore2/voice.h>
#include "WutEmulatorAudio.h"
#include "../AudioDriver.h"

//!Wii U AudioDriver: AX (sndcore2), 16 fixed AXVoice slots for one-shots
//!plus a dedicated stereo streaming path (two AXVoices, ring-buffered)
//!fed by a GuiSoundOggPlayer for the background stream.
class WutAudioDriver : public AudioDriver
{
	public:
		~WutAudioDriver() override;

		void init() override;
		void startMenuAudio() override;
		void stopMenuAudio() override;
		void startEmulatorAudio() override;
		void stopEmulatorAudio() override;
		void shutdown() override;

		WutEmulatorAudio* getEmulatorAudio() override { return emulatorAudio; }

	private:
		struct WutVoiceSlot {
			AXVoice* voice;
			bool active;
		};

		WutEmulatorAudio* emulatorAudio = nullptr;
		WutVoiceSlot voices[16];
		int nextVoiceSlot;
};
