/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlsmapperlive.cpp
 *
 * The real mapper behind PadMapper: the MAPPER_Pad* calls of sdl_mapper.cpp
 * (the same ones buttons.cpp uses), which all run on the main thread.
 ***************************************************************************/
#include "controlsmapper.h"
#include "dosboxsupport.h"
#include "settings.h"

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
