/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlssetupslive.cpp
 *
 * The real SetupHost: the setups folder beside dosbox.conf, the mapper file
 * the core is using, and the program the DOS PSP says is running.
 ***************************************************************************/
#include "controlssetupsfiles.h"
#include "dosboxsupport.h"
#include "storage.h"

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
