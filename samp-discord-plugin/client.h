#pragma once

#include <string>

namespace SAMP
{
	enum EConnectMode {
		SAMP_CONNECT_UNKNOWN,
		SAMP_CONNECT_SERVER,
		SAMP_CONNECT_DEBUG
	};

	struct ServerData {
		EConnectMode connect = SAMP_CONNECT_UNKNOWN;
		std::string address;
		std::string port;
		std::string username;
	};

	constexpr const char* ALLOWED_SERVER_HOST = "play.sarp.es";
	constexpr const char* ALLOWED_SERVER_PORT = "7777";

	bool readServerData(const char* cmdline, ServerData& data);
	bool isAllowedServer(const std::string& address, const std::string& port);
}
