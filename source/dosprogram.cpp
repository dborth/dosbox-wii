
/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * dosprogram.cpp
 ***************************************************************************/
#include "dosprogram.h"

std::string Program_PathFromEnvironment(const unsigned char * env, size_t size)
{
	if(!env || size < 4)
		return std::string();
	// the variables are strings up to an empty one
	size_t pos = 0;
	while(pos < size && env[pos] != 0)
	{
		while(pos < size && env[pos] != 0)
			pos++;
		pos++;	// past the string's end
	}
	if(pos >= size)
		return std::string();
	pos++;		// past the empty string
	if(pos + 2 > size || env[pos] != 1 || env[pos + 1] != 0)
		return std::string();
	pos += 2;
	std::string path;
	while(pos < size && env[pos] != 0)
		path += (char)env[pos++];
	if(pos >= size)
		return std::string();	// no end to the name: not a block we understand
	return path;
}

std::string Program_FromEnvironment(const unsigned char * env, size_t size)
{
	const std::string path = Program_PathFromEnvironment(env, size);
	// the file name alone: after the last separator, upper case, no spaces or characters a file name cannot hold
	size_t start = path.find_last_of("/\\:");
	start = start == std::string::npos ? 0 : start + 1;
	std::string out;
	for(size_t i = start; i < path.size(); i++)
	{
		const char c = path[i];
		if((unsigned char)c <= 32 || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
			continue;
		out += (char)((c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c);
	}
	return out;
}
