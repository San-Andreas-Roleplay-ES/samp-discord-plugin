#pragma once

#include <ctime>
#include <string>

namespace Discord
{
	void initialize();

	// partyMax <= 0 omits the party size bar from the profile entirely.
	void update(const time_t time, const char* details, const char* state, const char* largeImageText, int partySize, int partyMax);
	inline void update(const time_t time, const std::string& details, const std::string& state, const std::string& largeImageText, int partySize, int partyMax)
	{
		return update(time, details.c_str(), state.c_str(), largeImageText.c_str(), partySize, partyMax);
	}
}
