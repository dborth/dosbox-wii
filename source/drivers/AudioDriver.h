/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * AudioDriver.h
 *
 * Platform audio backend GuiSound delegates to. Exactly one driver
 * implements this and assigns the single global instance below
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include "EmulatorAudioDriver.h"

class AudioDriver
{
	public:
		virtual ~AudioDriver() = default;

		virtual void init() = 0;
		virtual void shutdown() = 0;
		virtual void startEmulatorAudio() = 0;
		virtual void stopEmulatorAudio() = 0;
		virtual void startMenuAudio() = 0;
		virtual void stopMenuAudio() = 0;

		//! The emulator-core-facing audio backend. Distinct from the
		//! voice/stream API below, which is used for menu sound effects
		//! and music rather than in-game emulator audio.
		virtual EmulatorAudioDriver* getEmulatorAudio() = 0;

		int32_t playVoice(const uint8_t* data, int32_t length, int volume) { return -1; }
		void stopVoice(int32_t voice) {}
		void pauseVoice(int32_t voice) {}
		void resumeVoice(int32_t voice) {}
		bool isVoicePlaying(int32_t voice) { return false; }
		void setVoiceVolume(int32_t voice, int volume) {}

		void playStream(const uint8_t* data, int32_t length, bool loop, int volume) {}
		void stopStream() {}
		void pauseStream() {}
		void resumeStream() {}
		bool isStreamPlaying() { return false; }
		void setStreamVolume(int volume) {}
};
