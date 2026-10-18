/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlslive.cpp
 *
 * The console-backed halves of the controller screens' seams, which need the
 * HAL and the DOSBox core (so the host tests use fakes instead):
 *   - the controllers, keyboard and mouse (ControlsHardware)
 *   - the mapper's pad sources and the joystick type (PadMapper)
 *   - the setups folder, the mapper file and the running program (SetupHost)
 ***************************************************************************/
#include "controlslive.h"
#include "controlsmapper.h"
#include "controlssetups.h"
#include "drivers/Platform.h"
#include "drivers/InputData.h"
#include "drivers/InputDriver.h"
#include "drivers/InputController.h"
#include "drivers/KeyboardDriver.h"
#include "drivers/MouseDriver.h"
#include "inputlogic.h"
#include "dosboxsupport.h"
#include "settings.h"
#include "storage.h"

// ---- hardware: the HAL's controllers, keyboard and mouse

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

// ---- mapper: the MAPPER_Pad* calls of sdl_mapper.cpp, which all run on the main thread

// What the mapper offers (sdl_mapper.cpp)
bool MAPPER_EventExists(const char * name);
int MAPPER_PadGetEvents(int pad, int source, char names[][16], int max, bool * from_stick);
bool MAPPER_PadSetEvent(int pad, int source, const char * event);
bool MAPPER_SaveBindsNow(void);

namespace {

class LiveMapper : public PadMapper
{
	public:
		bool eventExists(const char * name) const override { return MAPPER_EventExists(name); }

		std::vector<std::string> events(int pad, int source) const override
		{
			enum { MAX = 8 };
			char names[MAX][16];
			const int count = MAPPER_PadGetEvents(pad, source, names, MAX, nullptr);	// the count is of all of them, whatever MAX is
			std::vector<std::string> out;
			for(int i = 0; i < count && i < MAX; i++)
				out.push_back(names[i]);
			return out;
		}

		bool setEvent(int pad, int source, const char * event) override { return MAPPER_PadSetEvent(pad, source, event); }

		bool save() override { return MAPPER_SaveBindsNow(); }

		std::string joystickType() const override { return GetJoystickTypeName(); }

		bool setJoystickType(const std::string& type) override { return Settings_SetJoystickType(type.c_str()); }
};

} // namespace

PadMapper& Mapper_Live()
{
	static LiveMapper live;
	return live;
}

void Controls_JoystickTypeChanged()
{
	if(Mapper_MigrateJoystickBinds(Mapper_Live()))
		Mapper_Live().save();
}

// ---- setups: the folder beside dosbox.conf, the mapper file the core is using, the program the DOS PSP says is running

namespace {

class LiveSetups : public FileSetupHost
{
	public:
		// <app drive>/DOSBox/Setups: next to dosbox.conf, so -conf finds a setup's conf there too
		std::string directory() const override
		{
			return appDrive[0] ? std::string(appDrive) + "/" DOSBOX_DIR_NAME "/Setups" : std::string();
		}

		std::string mapperFile() const override
		{
			char path[256];
			return GetMapperFilePath(path, sizeof(path)) ? std::string(path) : std::string();
		}

		std::string runningProgram() const override
		{
			char name[64];
			return GetRunningProgram(name, sizeof(name)) ? std::string(name) : std::string();
		}
};

} // namespace

SetupHost& Setups_Live()
{
	static LiveSetups live;
	return live;
}
