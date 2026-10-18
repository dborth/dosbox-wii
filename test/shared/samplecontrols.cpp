/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * samplecontrols.cpp
 *
 * Invented players and a model over them, for the host tests and previews.
 * Not part of the console build: the app installs the mapper-backed model.
 ***************************************************************************/
#include "samplecontrols.h"
#include <ctype.h>
#include "controllerlayout.h"
#include "drivers/InputData.h"
#include <deque>
#include <map>
#include <stdint.h>
#include <stdio.h>

namespace {

bool hasSource(int control) { return Pad_SourceOfControl(control) >= 0; }

// ---- hardware: what the screens read from the console

// Invented players for the host tests and previews, until the real hardware is installed. A test or preview
// can make a player something else, and set what is held.
class SampleHardware : public ControlsHardware
{
	public:
		SampleHardware() : heldBits(0) {}

		PlayerHardware player(int i) const override
		{
			PlayerHardware h;
			if(i >= 0 && i < 4 && over[i].set)
			{
				h.type = over[i].type;
				h.devices = over[i].devices;
				h.connected = h.type != PadType::NONE;
				h.sideways = over[i].sideways;
				h.stickClicks = over[i].stickClicks;
				h.battery = h.connected && h.type != PadType::GAMECUBE ? 80 : -1;
				return h;
			}
			switch(i)
			{
				case 0: h.connected = true; h.type = PadType::WIIMOTE_NUNCHUK; h.battery = 80; break;
				case 1: h.connected = true; h.type = PadType::CLASSIC; h.battery = 45; break;
				case 2: h.connected = true; h.type = PadType::GAMECUBE; break;
				default: break;
			}
			return h;
		}

		DeviceStatus devices() const override
		{
			DeviceStatus d;
			d.keyboard = true;
			d.mouse = false;
			d.wiimoteIr = false;
			d.gamepadPresent = false;
			d.gamepadTouch = false;
			return d;
		}

		uint32_t held(int player) const override { return player == 0 ? heldBits : 0; }

		void setHeld(uint32_t bits) { heldBits = bits; }
		void setHardware(int player, PadType type, bool sideways, bool stickClicks)
		{
			if(player < 0 || player >= 4)
				return;
			over[player].set = true;
			over[player].type = type;
			over[player].sideways = sideways;
			over[player].stickClicks = stickClicks;
			over[player].devices.clear();
		}
		void setDevices(int player, const std::vector<PadType>& list, bool sideways, bool stickClicks)
		{
			if(list.empty() || player < 0 || player >= 4)
				return;
			setHardware(player, list[0], sideways, stickClicks);
			over[player].devices = list;
		}
		void resetHardware()
		{
			for(int i = 0; i < 4; i++)
				over[i].set = false;
		}

	private:
		struct Override { bool set = false; PadType type = PadType::NONE; bool sideways = false; bool stickClicks = false; std::vector<PadType> devices; };
		Override over[4];
		uint32_t heldBits;
};

SampleHardware * sampleHardware()
{
	static SampleHardware h;
	return &h;
}

ControlsHardware * hardwareInstalled = nullptr;

const ControlsHardware& hardware()
{
	return hardwareInstalled ? *hardwareInstalled : *sampleHardware();
}

class SampleControlsModel : public ControlsModel
{
	public:
		SampleControlsModel() : mode(0), dirty(false)
		{
			for(int i = 0; i < 4; i++)
				preset[i] = PRESET_ARROWS;
		}

		int playerCount() const override { return 4; }

		PlayerInfo player(int i) const override
		{
			const bool ok = validPlayer(i);
			return Controls_ComposePlayer(hardware().player(i), i, Emulated_Modes()[(size_t)mode],
				ok ? Presets_All()[(size_t)preset[i]].name : std::string());
		}

		DeviceStatus devices() const override { return hardware().devices(); }

		uint32_t held(int player) const override { return hardware().held(player); }

		std::vector<EmulatedMode> emulatedModes() const override { return Emulated_Modes(); }
		int currentEmulated() const override { return mode; }
		void setEmulated(int index) override
		{
			if(index >= 0 && index < (int)Emulated_Modes().size() && index != mode)
			{
				mode = index;
				dirty = true;
			}
		}

		std::vector<PresetInfo> presets() const override { return Presets_All(); }
		int currentPreset(int player) const override { return validPlayer(player) ? preset[player] : 0; }
		void applyPreset(int player, int index) override
		{
			if(!validPlayer(player) || index < 0 || index >= PRESET_COUNT)
				return;
			preset[player] = index;
			dropEdits(player); // a preset brings its own defaults, for this player only
			dirty = true;
		}

		bool supported(int control) const override { return hasSource(control); }

		BindingTarget binding(int player, int control) const override
		{
			const std::map<int64_t, BindingTarget>::const_iterator it = edits.find(editKey(player, control));
			if(it != edits.end())
				return it->second;
			return hasSource(control) ? Presets_Target(validPlayer(player) ? preset[player] : 0, control) : BindingTarget();
		}
		BindingTarget presetBinding(int p, int control) const override
		{
			return p >= 0 && p < PRESET_COUNT && hasSource(control) ? Presets_Target(p, control) : BindingTarget();
		}
		bool modified(int player) const override
		{
			for(std::map<int64_t, BindingTarget>::const_iterator it = edits.begin(); it != edits.end(); ++it)
				if(it->first / kStride == (int64_t)player)
					return true;
			return false;
		}

		BindingTarget defaultBinding(int player, int control) const override { return hasSource(control) ? Presets_Target(validPlayer(player) ? preset[player] : 0, control) : BindingTarget(); }
		void setBinding(int player, int control, const BindingTarget& t) override
		{
			if(!hasSource(control) || binding(player, control) == t)
				return;
			edits[editKey(player, control)] = t;
			dirty = true;
		}
		void resetBindings(int player) override
		{
			if(modified(player))
			{
				dropEdits(player);
				dirty = true;
			}
		}

		std::vector<SetupInfo> setups() const override
		{
			std::vector<SetupInfo> v;
			for(size_t i = 0; i < saved.size(); i++)
			{
				SetupInfo s;
				s.name = saved[i].name;
				s.program = saved[i].program;
				s.emulated = Emulated_Modes()[(size_t)saved[i].mode].name;
				s.preset = presetSummary(saved[i].preset);
				v.push_back(s);
			}
			return v;
		}
		bool saveSetup(const std::string& name, const std::string& program) override
		{
			if(name.empty())
				return false;
			Setup s;
			s.name = name;
			s.program = program;
			s.mode = mode;
			for(int i = 0; i < 4; i++)
				s.preset[i] = preset[i];
			s.edits = edits;
			current = name;
			dirty = false;
			for(size_t i = 0; i < saved.size(); i++)
			{
				if(saved[i].name == name)
				{
					saved[i] = s;
					return true;
				}
			}
			saved.push_back(s);
			return true;
		}
		bool loadSetup(int index) override
		{
			if(index < 0 || (size_t)index >= saved.size())
				return false;
			mode = saved[(size_t)index].mode;
			for(int i = 0; i < 4; i++)
				preset[i] = saved[(size_t)index].preset[i];
			edits = saved[(size_t)index].edits;
			current = saved[(size_t)index].name;
			dirty = false;
			return true;
		}
		std::string runningProgram() const override { return "KEEN4E.EXE"; }
		std::string setupName() const override { return current; }
		bool setupModified() const override { return dirty; }

		std::vector<BindingTarget> suggestions(int player, int control) const override { return Presets_Suggestions(*this, player, control); }

		void beginCapture() override { pending.clear(); }
		bool pollCapture(CaptureEvent& e) override
		{
			if(pending.empty())
				return false;
			e = pending.front();
			pending.pop_front();
			return true;
		}
		void pushCapture(const CaptureEvent& e) { pending.push_back(e); }

	private:
		struct Setup
		{
			std::string name, program;
			int mode;
			int preset[4];
			std::map<int64_t, BindingTarget> edits;
		};

		static bool validPlayer(int player) { return player >= 0 && player < 4; }

		void dropEdits(int player)
		{
			for(std::map<int64_t, BindingTarget>::iterator it = edits.begin(); it != edits.end();)
			{
				if(it->first / kStride == (int64_t)player)
					edits.erase(it++);
				else
					++it;
			}
		}

		// the preset every connected player started from, or "Mixed" when they differ
		std::string presetSummary(const int (&p)[4]) const
		{
			int first = -1;
			for(int i = 0; i < 4; i++)
			{
				if(!player(i).connected)
					continue;
				if(first < 0)
					first = p[i];
				else if(p[i] != first)
					return "Mixed";
			}
			return Presets_All()[(size_t)(first < 0 ? p[0] : first)].name;
		}

		static const int64_t kStride = (int64_t)1 << 32;
		static int64_t editKey(int player, int control) { return (int64_t)player * kStride + (int64_t)(uint32_t)control; }

		int mode;
		int preset[4];
		bool dirty;
		std::string current;
		std::map<int64_t, BindingTarget> edits;
		std::vector<Setup> saved;
		std::deque<CaptureEvent> pending;
};

SampleControlsModel * sampleModel()
{
	static SampleControlsModel m;
	return &m;
}

} // namespace



void Controls_SetHardware(ControlsHardware * hardware)
{
	hardwareInstalled = hardware;
}

void SampleControls_SetHeld(uint32_t bits)
{
	sampleHardware()->setHeld(bits);
}

void SampleControls_PushCapture(const CaptureEvent& event)
{
	if(&Controls() == sampleModel())
		sampleModel()->pushCapture(event);
}

void SampleControls_SetHardware(int player, PadType type, bool sideways, bool stickClicks)
{
	sampleHardware()->setHardware(player, type, sideways, stickClicks);
}

void SampleControls_SetDevices(int player, const std::vector<PadType>& devices, bool sideways, bool stickClicks)
{
	sampleHardware()->setDevices(player, devices, sideways, stickClicks);
}

void SampleControls_ResetHardware()
{
	sampleHardware()->resetHardware();
}

void SampleControls_Use()
{
	Controls_SetFallback(sampleModel());
}
