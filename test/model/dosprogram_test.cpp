// Host tests for the DOS program reader (dosprogram.cpp). The text checks tie the block's layout to DOS_Execute's own source.
#include "dosprogram.h"
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <vector>

static int failures = 0, checks = 0;

#define CHECK(cond) do { checks++; if(!(cond)) { failures++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while(0)
#define CHECK_EQ(a, b) do { checks++; const std::string x = (a), y = (b); if(x != y) { failures++; printf("FAIL %s:%d  %s\n      got  \"%s\"\n      want \"%s\"\n", __FILE__, __LINE__, #a, x.c_str(), y.c_str()); } } while(0)

static std::string readFile(const std::string& rel)
{
	std::ifstream f(std::string(SRC_DIR) + "/" + rel);
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

static std::vector<unsigned char> bytes(const char * text, size_t n) { return std::vector<unsigned char>(text, text + n); }

static void testProgramFromEnvironment()
{
	// the layout DOS_Execute's MakeEnv writes: variables, an empty string, the word 1, the full path
	static const char withVars[] = "PATH=Z:\\\0COMSPEC=Z:\\COMMAND.COM\0\0\1\0C:\\GAMES\\Keen4e.exe\0";
	std::vector<unsigned char> env = bytes(withVars, sizeof(withVars));
	CHECK_EQ(Program_FromEnvironment(&env[0], env.size()), "KEEN4E.EXE");
	// with room after it, as the block is bigger than what is written
	env.resize(env.size() + 40, 0);
	CHECK_EQ(Program_FromEnvironment(&env[0], env.size()), "KEEN4E.EXE");
	// no variables at all: MakeEnv writes one zero, then the word
	static const char none[] = "\0\1\0D:\\X.COM\0";
	env = bytes(none, sizeof(none));
	CHECK_EQ(Program_FromEnvironment(&env[0], env.size()), "X.COM");
	// a path with no end, a missing word, a block too short: nothing
	static const char cut[] = "A=1\0\0\1\0C:\\GAME.EXE";
	env = bytes(cut, sizeof(cut) - 1);
	CHECK_EQ(Program_FromEnvironment(&env[0], env.size()), "");
	static const char noWord[] = "A=1\0\0\2\0C:\\GAME.EXE\0";
	env = bytes(noWord, sizeof(noWord));
	CHECK_EQ(Program_FromEnvironment(&env[0], env.size()), "");
	static const char noEnd[] = "A=1\0B=2";
	env = bytes(noEnd, sizeof(noEnd) - 1);
	CHECK_EQ(Program_FromEnvironment(&env[0], env.size()), "");
	CHECK_EQ(Program_FromEnvironment(nullptr, 10), "");
	CHECK_EQ(Program_FromEnvironment(&env[0], 2), "");
	// the empty path is no program
	static const char emptyPath[] = "\0\1\0\0";
	env = bytes(emptyPath, sizeof(emptyPath));
	CHECK_EQ(Program_FromEnvironment(&env[0], env.size()), "");

	// MakeEnv in the tree is what writes it, in this order
	const std::string exec = readFile("dosbox/dos/dos_execute.cpp");
	const size_t make = exec.find("static bool MakeEnv");
	const size_t word = exec.find("mem_writew(envwrite,1);", make);
	const size_t path = exec.find("DOS_Canonicalize(name,namebuf)", make);
	CHECK(make != std::string::npos && word != std::string::npos && path != std::string::npos && word < path);
	CHECK(exec.find("MakeEnv(name,&envseg)") != std::string::npos);
}

// the file name alone, upper case, with the characters a file name cannot hold dropped
static void testFileName()
{
	static const char spaced[] = "\0\1\0c:\\my games\\keen 4.exe\0";
	std::vector<unsigned char> env(spaced, spaced + sizeof(spaced));
	CHECK_EQ(Program_PathFromEnvironment(&env[0], env.size()), "c:\\my games\\keen 4.exe");
	CHECK_EQ(Program_FromEnvironment(&env[0], env.size()), "KEEN4.EXE");
	static const char slashes[] = "\0\1\0/games/doom/doom.exe\0";
	env.assign(slashes, slashes + sizeof(slashes));
	CHECK_EQ(Program_FromEnvironment(&env[0], env.size()), "DOOM.EXE");
	static const char drive[] = "\0\1\0c:\0";
	env.assign(drive, drive + sizeof(drive));
	CHECK_EQ(Program_FromEnvironment(&env[0], env.size()), "");
}

int main()
{
	testProgramFromEnvironment();
	testFileName();
	printf("%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
