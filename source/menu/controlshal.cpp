/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlshal.cpp
 ***************************************************************************/
#include "controlshal.h"
#include "drivers/Platform.h"
#include "drivers/InputData.h"
#include "drivers/InputDriver.h"
#include "drivers/InputController.h"
#include "drivers/KeyboardDriver.h"
#include "drivers/MouseDriver.h"
#include "inputlogic.h"

namespace {

const int CHANNELS = 4;

class HalControlsHardware : public ControlsHardware
{
	public:
		PlayerHardware player(int index) const override
		{
			PlayerHardware h;
			if(!valid(index))
				return h;
			const InputPadData& pad = controller[index]->getPadData();
			h.type = PadType_FromPad(pad);
			h.connected = h.type != PadType::NONE;
			h.devices = PadDevices_FromPad(pad);
			h.battery = PadBattery(pad, h.type);
			// only a lone Wiimote is held sideways (the driver clears the flag for anything with an extension)
			h.sideways = h.type == PadType::WIIMOTE && controller[index]->isSideways();
			h.stickClicks = platform && platform->getInput() && platform->getInput()->reportsStickClick();
			return h;
		}

		DeviceStatus devices() const override
		{
			DeviceStatus d;
			d.keyboard = keyboard && keyboard->isConnected();
			d.mouse = usbMouse && usbMouse->isConnected();
			d.wiimoteIr = false;
			for(int i = 0; i < CHANNELS; i++)
			{
				const InputPadData& pad = controller[i]->getPadData();
				if(pad.hw_connected[INPUT_HW_WIIMOTE] && pad.validPointer && !pad.isTouch)
					d.wiimoteIr = true;
			}
			// the GamePad is channel 0's DRC profile
			const InputPadData& drc = controller[0]->getPadData();
			d.gamepadPresent = drc.hw_connected[INPUT_HW_DRC];
			d.gamepadTouch = d.gamepadPresent && drc.validPointer && drc.isTouch;
			d.gamepadBattery = d.gamepadPresent ? (int)(drc.hw_battery_level[INPUT_HW_DRC] > 100 ? 100 : drc.hw_battery_level[INPUT_HW_DRC]) : -1;
			return d;
		}

		uint32_t held(int index) const override
		{
			if(!valid(index))
				return 0;
			return PadButtonsForDos(controller[index]->getPadData(), controller[index]->isSideways());
		}

		void beginCapture() override
		{
			// the shim polls both only while emulating, so what is queued now is from before the menu
			KeyEvent ke;
			while(keyboard && keyboard->poll(ke)) {}
			MouseEvent me;
			mouseHeld = 0;
			while(usbMouse && usbMouse->poll(me))
				mouseHeld = me.buttons;
		}

		bool pollCapture(CaptureEvent& event) override
		{
			KeyEvent ke;
			while(keyboard && keyboard->poll(ke))
			{
				if(!ke.pressed)
					continue;
				event.kind = TARGET::KEY;
				event.code = Catalog_KeyFromHid(ke.hidUsage);	// -1 for a key the mapper cannot bind
				return true;
			}
			// a mouse event carries every button held, so a press is a bit that was not set in the one before
			MouseEvent me;
			while(usbMouse && usbMouse->poll(me))
			{
				const uint8_t fresh = (uint8_t)(me.buttons & ~mouseHeld);
				mouseHeld = me.buttons;
				if(!fresh)
					continue;
				event.kind = TARGET::MOUSE;
				event.code = (fresh & MOUSE_BTN_LEFT) ? 0 : (fresh & MOUSE_BTN_RIGHT) ? 1 : 2;	// the catalog's order: left, right, middle
				return true;
			}
			return false;
		}

	private:
		static bool valid(int index) { return index >= 0 && index < CHANNELS && controller[index]; }
		uint8_t mouseHeld = 0;
};

} // namespace

ControlsHardware& Controls_Hal()
{
	static HalControlsHardware hal;
	return hal;
}
