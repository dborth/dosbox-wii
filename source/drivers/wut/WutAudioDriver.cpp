/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutAudioDriver.cpp
 ***************************************************************************/
#include <coreinit/cache.h>
#include <string.h>
#include <unistd.h>
#include <proc_ui/procui.h>

#include "../Platform.h"
#include "WutAudioDriver.h"

static WutAudioDriver *instance = nullptr;

// Once the OS has taken the foreground away from us (HOME menu overlay,
// forced exit, etc.) we stop feeding/starting audio rather than continuing
// to drive AX in the background.
static inline bool isForeground() { return platform->getStatus() == Status::Running; }

static void wut_frame_callback() {
	if (instance)
	{
		if (instance->getEmulatorAudio())
			instance->getEmulatorAudio()->frameTick();
	}
}

template <int N>
static void buildChannelMix(AXVoiceDeviceMixData (&mix)[N], bool left, bool right) {
	memset(mix, 0, sizeof(mix));
	if (left)
		mix[0].bus[0].volume = 0x8000;
	if (right && N > 1)
		mix[1].bus[0].volume = 0x8000;
}

static constexpr int AX_TV_CHANNELS = 6;
static constexpr int AX_DRC_CHANNELS = 4;

void WutAudioDriver::init() {
	instance = this;
	AXInitParams params = {};
	params.renderer = AX_INIT_RENDERER_48KHZ;
	params.pipeline = AX_INIT_PIPELINE_SINGLE;
	AXInitWithParams(&params);
	AXRegisterAppFrameCallback(wut_frame_callback);

	nextVoiceSlot = 0;

	// Pre-allocate the 16 hardware SFX voices - mono, centered equally on
	// both output channels of both devices.
	AXVoiceDeviceMixData tvMixCentered[AX_TV_CHANNELS];
	AXVoiceDeviceMixData drcMixCentered[AX_DRC_CHANNELS];
	buildChannelMix(tvMixCentered, true, true);
	buildChannelMix(drcMixCentered, true, true);

	for (int i = 0; i < 16; i++) {
		voices[i].voice = AXAcquireVoice(31, 0, 0);
		voices[i].active = false;
		if (voices[i].voice) {
			AXVoiceBegin(voices[i].voice);
			AXSetVoiceType(voices[i].voice, 0);

			AXSetVoiceDeviceMix(voices[i].voice, AX_DEVICE_TYPE_TV, 0, tvMixCentered);
			AXSetVoiceDeviceMix(voices[i].voice, AX_DEVICE_TYPE_DRC, 0, drcMixCentered);
			AXVoiceEnd(voices[i].voice);
		}
	}

	emulatorAudio = new WutEmulatorAudio();
	emulatorAudio->init();
}

WutAudioDriver::~WutAudioDriver() {
	delete emulatorAudio;
}

void WutAudioDriver::startMenuAudio() {

}

void WutAudioDriver::stopMenuAudio() {

}

void WutAudioDriver::startEmulatorAudio() {
	emulatorAudio->start();
}

void WutAudioDriver::stopEmulatorAudio() {
	emulatorAudio->stop();
}

void WutAudioDriver::shutdown() {
	stopEmulatorAudio();
	emulatorAudio->shutdown();
	AXDeregisterAppFrameCallback(wut_frame_callback);

	for (int i = 0; i < 16; i++) {
		if (voices[i].voice) {
			AXFreeVoice(voices[i].voice);
			voices[i].voice = nullptr;
		}
	}

	AXQuit();
	instance = nullptr;
}
