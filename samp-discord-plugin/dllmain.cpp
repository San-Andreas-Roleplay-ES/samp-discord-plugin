#include <process.h>
#include "client.h"
#include "discord.h"
#include "query.h"

static void process(void*)
{
	SAMP::ServerData data;
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
	if (SAMP::readServerData(GetCommandLine(), data)) {
		if (data.connect == SAMP::SAMP_CONNECT_SERVER && !SAMP::isAllowedServer(data.address, data.port)) {
			return;
		}

		Discord::initialize();

		auto start = std::time(0);
		if (data.connect == SAMP::SAMP_CONNECT_SERVER) {
			SAMP::Query query(data.address, std::stoi(data.port));
			while (true) {
				SAMP::Query::Information information;
				if (query.info(information)) {
					auto fullAddress = data.address + ':' + data.port;
					auto invite = std::string("Juega ahora en San Andreas Roleplay");
					Discord::update(start, information.hostname, fullAddress, invite, information.basic.players, information.basic.maxPlayers);
					Sleep(15000-QUERY_DEFAULT_TIMEOUT*2);
				}
			}
		}
		else if (data.connect == SAMP::SAMP_CONNECT_DEBUG) {
			while (true) {
				Discord::update(start, "Debug server", "localhost", "Playing debug mode", 1, 1);
				Sleep(15000);
			}
		}
	}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
		case DLL_PROCESS_ATTACH: {
			DisableThreadLibraryCalls(module);
			_beginthread(&process, 0, nullptr);
			break;
		}
		case DLL_PROCESS_DETACH: {
			WSACleanup();
			break;
		}
    }
    return TRUE;
}
