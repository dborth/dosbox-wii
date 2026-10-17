/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2009-2026
 * WiiAudioDriver.h
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include "../../AudioDriver.h"
#include "../OgcEmulatorAudio.h"

class WiiAudioDriver : public AudioDriver
{
	public:
		~WiiAudioDriver() override;

		void init() override;
		void shutdown() override;
		void startMenuAudio() override;
		void stopMenuAudio() override;
		void startEmulatorAudio() override;
		void stopEmulatorAudio() override;

		OgcEmulatorAudio* getEmulatorAudio() override { return emulatorAudio; }

	private:
		int streamVolume;
		OgcEmulatorAudio* emulatorAudio = nullptr;
};
