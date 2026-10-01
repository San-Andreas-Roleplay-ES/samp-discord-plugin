#include "discord.h"
#include <Windows.h>
#include <cstdint>

// Minimal Discord IPC client: talks directly to Discord's local RPC named pipe.
// Replaces the old discord-rpc.lib, which predates Rich Presence buttons and
// has no way to send them.
namespace Discord
{
	namespace
	{
		constexpr const char* APPLICATION_ID = "1378601547751231578";
		constexpr const char* DISCORD_INVITE_URL = "https://discord.gg/E5tPBwWnn9";
		constexpr const char* UCP_URL = "https://ucp.sarp.es";
		constexpr DWORD HANDSHAKE_TIMEOUT_MS = 1000;
		constexpr DWORD UPDATE_TIMEOUT_MS = 500;

		enum Opcode : uint32_t {
			OPCODE_HANDSHAKE = 0,
			OPCODE_FRAME = 1,
		};

#pragma pack(push, 1)
		struct FrameHeader {
			uint32_t opcode;
			uint32_t length;
		};
#pragma pack(pop)

		HANDLE pipe = INVALID_HANDLE_VALUE;

		void appendJsonEscaped(std::string& out, const char* input)
		{
			for (const char* c = input; *c; ++c) {
				switch (*c) {
					case '"': out += "\\\""; break;
					case '\\': out += "\\\\"; break;
					case '\n': out += "\\n"; break;
					case '\r': out += "\\r"; break;
					case '\t': out += "\\t"; break;
					default:
						out += *c;
				}
			}
		}

		// Picks the seasonal Rich Presence asset key for the current calendar
		// month. Each key must be uploaded under that exact name in the
		// Discord app's Rich Presence assets, or Discord will just show no
		// image during that month.
		std::string seasonalImageKey()
		{
			time_t now = std::time(nullptr);
			tm local;
			localtime_s(&local, &now);

			switch (local.tm_mon + 1) {
				case 1: return "logo_anio_nuevo";
				case 2: return "logo_san_valentin";
				case 3:
				case 4:
				case 5: return "logo_primavera";
				case 6:
				case 7:
				case 8: return "logo_verano";
				case 9: return "logo_otono";
				case 10: return "logo_halloween";
				case 11: return "logo_otono";
				case 12: return "logo_navidad";
				default: return "logo";
			}
		}

		void disconnect()
		{
			if (pipe != INVALID_HANDLE_VALUE) {
				CloseHandle(pipe);
				pipe = INVALID_HANDLE_VALUE;
			}
		}

		bool writeFrame(uint32_t opcode, const std::string& json)
		{
			if (pipe == INVALID_HANDLE_VALUE) {
				return false;
			}

			FrameHeader header{ opcode, static_cast<uint32_t>(json.size()) };
			std::string buffer(reinterpret_cast<char*>(&header), sizeof(header));
			buffer += json;

			DWORD written = 0;
			bool ok = WriteFile(pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &written, nullptr) && written == buffer.size();
			if (!ok) {
				disconnect();
			}

			return ok;
		}

		// Discord echoes every command back; drain that echo so its output
		// buffer never backs up during a long play session. Bounded by
		// timeoutMs so a stalled Discord instance can't hang this thread.
		void drainResponse(DWORD timeoutMs)
		{
			if (pipe == INVALID_HANDLE_VALUE) {
				return;
			}

			DWORD available = 0;
			for (DWORD waited = 0; waited < timeoutMs; waited += 10) {
				if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) {
					disconnect();
					return;
				}
				if (available >= sizeof(FrameHeader)) {
					break;
				}
				Sleep(10);
			}

			if (available < sizeof(FrameHeader)) {
				return;
			}

			FrameHeader header;
			DWORD read = 0;
			if (!ReadFile(pipe, &header, sizeof(header), &read, nullptr) || read != sizeof(header)) {
				disconnect();
				return;
			}

			char discard[4096];
			for (DWORD remaining = header.length; remaining > 0;) {
				DWORD chunk = remaining < sizeof(discard) ? remaining : sizeof(discard);
				if (!ReadFile(pipe, discard, chunk, &read, nullptr) || read == 0) {
					disconnect();
					return;
				}
				remaining -= read;
			}
		}

		bool ensureConnected()
		{
			if (pipe != INVALID_HANDLE_VALUE) {
				return true;
			}

			for (int i = 0; i < 10 && pipe == INVALID_HANDLE_VALUE; ++i) {
				std::string name = "\\\\.\\pipe\\discord-ipc-" + std::to_string(i);
				pipe = CreateFileA(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
			}

			if (pipe == INVALID_HANDLE_VALUE) {
				return false;
			}

			std::string handshake = "{\"v\":1,\"client_id\":\"" + std::string(APPLICATION_ID) + "\"}";
			if (!writeFrame(OPCODE_HANDSHAKE, handshake)) {
				return false;
			}

			drainResponse(HANDSHAKE_TIMEOUT_MS);
			return pipe != INVALID_HANDLE_VALUE;
		}
	}

	void initialize()
	{
		ensureConnected();
	}

	void update(const time_t time, const char* details, const char* state, const char* largeImageText, int partySize, int partyMax)
	{
		if (!ensureConnected()) {
			return;
		}

		std::string json = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":";
		json += std::to_string(GetCurrentProcessId());
		json += ",\"activity\":{\"details\":\"";
		appendJsonEscaped(json, details);
		json += "\",\"state\":\"";
		appendJsonEscaped(json, state);
		json += "\",\"timestamps\":{\"start\":";
		json += std::to_string(static_cast<long long>(time));
		json += "}";

		if (partyMax > 0) {
			json += ",\"party\":{\"id\":\"sarp-server\",\"size\":[";
			json += std::to_string(partySize);
			json += ",";
			json += std::to_string(partyMax);
			json += "]}";
		}

		json += ",\"assets\":{\"large_image\":\"";
		json += seasonalImageKey();
		json += "\",\"large_text\":\"";
		appendJsonEscaped(json, largeImageText);
		json += "\"},\"buttons\":[{\"label\":\"\xC3\x9anete al Discord\",\"url\":\"";
		json += DISCORD_INVITE_URL;
		json += "\"},{\"label\":\"Visita la UCP\",\"url\":\"";
		json += UCP_URL;
		json += "\"}]}},\"nonce\":\"";
		json += std::to_string(GetTickCount64());
		json += "\"}";

		if (writeFrame(OPCODE_FRAME, json)) {
			drainResponse(UPDATE_TIMEOUT_MS);
		}
	}
}
