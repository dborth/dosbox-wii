/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2026
 * WutEmulatorAudio.h
 *
 * AX-backed EmulatorAudioDriver implementation for Wii U. Feeds the core's
 * mixer output through a pair of hard-panned AX voices (L/R) reading a
 * looping ring buffer, with dynamic rate control.
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include <sndcore2/voice.h>
#include "../EmulatorAudioDriver.h"

class WutEmulatorAudio : public EmulatorAudioDriver
{
	public:
		//! Output format. The core's mixer must be configured to produce
		//! exactly this (signed 16-bit, stereo, interleaved L/R, host
		//! endian). Same rate as the Wii driver, so the mixer and the OPL/GUS/
		//! PC speaker/Tandy rates behave identically on both. AX still mixes at
		//! 48 kHz: each voice upsamples this to 48 kHz with AX's linear SRC, at
		//! ratio SAMPLE_RATE / AXGetInputSamplesPerSec() (see configureVoice).
		static constexpr int SAMPLE_RATE = 32000;

		//! Stereo frames per buffer returned by getWriteBuffer(). 544 frames is
		//! 17.0 ms at 32 kHz, matching OgcEmulatorAudio, so the queue
		//! thresholds below keep their tuned timing.
		static constexpr int FRAMES_PER_BUFFER = 544;

		WutEmulatorAudio();
		~WutEmulatorAudio() override;

		// Acquires the two AX voices and configures their fixed (never
		// touched again) format/loop/mix/SRC-bypass settings. Must be
		// called after AXInitWithParams() -- ie. from WutAudioDriver::init().
		void init() override;
		void resetAudio() override;
		int getUnplayed() override;

		//! No-op. Voices are armed lazily by commitWrite() itself, once
		//! enough is queued
		void start() {}

		double getDynamicRate() override;
		bool canWrite() override;
		uint16_t* getWriteBuffer() override;
		void commitWrite() override;

		int getSampleRate() override { return SAMPLE_RATE; }
		int getFramesPerBuffer() override { return FRAMES_PER_BUFFER; }

		//! Hard-stops both voices (leaving to the menu, or shutdown).
		void stop();

		//! Frees the two AX voices. Must be called before AXQuit().
		void shutdown();

		//! AX frame-callback hook (~3ms cadence)
		void frameTick();

	private:
		// One commitWrite() chunk
		static constexpr int COMMIT_FRAMES = FRAMES_PER_BUFFER;

		// Ring capacity / thresholds, in frames
		static constexpr int RING_FRAMES        = 16 * COMMIT_FRAMES; // 8704 (~272ms)
		static constexpr int MAX_QUEUED_FRAMES  = 12 * COMMIT_FRAMES; // 4-buffer safety zone
		static constexpr int HIGH_WATER_FRAMES  = 8  * COMMIT_FRAMES;
		static constexpr int HIGH_RELEASE_FRAMES= 6  * COMMIT_FRAMES;
		static constexpr int LOW_RELEASE_FRAMES = 6  * COMMIT_FRAMES;
		static constexpr int LOW_WATER_FRAMES   = 4  * COMMIT_FRAMES;
		static constexpr int CRITICAL_FRAMES    = 1  * COMMIT_FRAMES;
		static constexpr int HIGH_CRITICAL_FRAMES = 11 * COMMIT_FRAMES;

		// Pre-roll level before (re)starting the voices
		static constexpr int START_LEVEL_FRAMES = 6  * COMMIT_FRAMES;

		static constexpr double RATE_SLOW_DOWN = 1.005;
		static constexpr double RATE_SPEED_UP = 0.995;
		static constexpr double RATE_EMERGENCY_SLOW_DOWN = 1.015;
		static constexpr double RATE_EMERGENCY_SPEED_UP = 0.985;
		static constexpr double RATE_NEUTRAL = 1.0;

		// AXSetVoiceDeviceMix output channel counts (TV/DRC)
		static constexpr int AX_TV_CHANNELS = 6;
		static constexpr int AX_DRC_CHANNELS = 4;

		// Static full-volume envelope
		static constexpr uint16_t AX_MAX_VOLUME = 0x8000;

		enum RateState { RATE_STATE_NEUTRAL, RATE_STATE_DRAINING, RATE_STATE_FILLING };

		void configureVoice(AXVoice* v, int16_t* ringBuf, bool isLeft);
		void writeFrames(const int16_t* interleavedSrc, uint32_t frames);

		//! Resyncs both voices' hardware offset to the oldest sample still
		//! queued and sets them PLAYING
		void startVoice();

		int getUnplayedBuffers() const { return (int)(queuedFrames / COMMIT_FRAMES); }

		// Source frames the voices consume per AX frame callback: one tick's
		// output samples (AXGetInputSamplesPerFrame) scaled by the SRC ratio.
		// 96 for 144 output samples at 32 kHz -> 48 kHz.
		uint32_t srcFramesPerTick = 0;

		// Stop-on-underrun margin, in source frames
		uint32_t minFrames = 0;

		AXVoice* voiceL = nullptr;
		AXVoice* voiceR = nullptr;

		alignas(32) int16_t ringL[RING_FRAMES];
		alignas(32) int16_t ringR[RING_FRAMES];

		// The core writes one interleaved COMMIT_FRAMES chunk here per
		// commitWrite() call; commitWrite() deinterleaves it into ringL/R.
		alignas(32) int16_t stagingInterleaved[COMMIT_FRAMES * 2];

		// Owned exclusively by commitWrite()/the main thread.
		uint32_t writePos = 0;

		// Software-owned queue depth
		volatile uint32_t queuedFrames = 0;

		// Whether the AX voices are PLAYING right now
		volatile bool voiceRunning = false;

		bool primed = false;

		RateState rateState = RATE_STATE_NEUTRAL;
};
