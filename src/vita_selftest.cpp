// SPDX-License-Identifier: LGPL-2.1-or-later
// Opt-in diagnostics use the same compiled code as the multiplayer client.
#include "porting_vita.h"
#include "porting.h"
#include "network/socket.h"
#include "network/address.h"
#include "network/connection.h"
#include "network/networkpacket.h"
#include "client/inputhandler.h"
#include "util/vita_text.h"
#include "util/auth.h"
#include "util/srp.h"
#include "util/hashing.h"
#include "util/hex.h"
#include "database/database-sqlite3.h"
#include "filesys.h"
#include "threading/semaphore.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <memory>

namespace porting {
static void testSrp(FILE *report)
{
	const std::string salt = "vita-selftest-salt";
	const std::string password = "test password";
	const char *name = "vitadiagnostic";
	const std::string verifier = generate_srp_verifier(name, password, salt);
	// The host harness independently checks this against Python SHA-256/pow.
	std::fprintf(report, "SRP verifier SHA256 %s\n", hex_encode(hashing::sha256(verifier)).c_str());
	std::unique_ptr<SRPUser, decltype(&srp_user_delete)> user(
		srp_user_new(SRP_SHA256, SRP_NG_2048, name, name,
			reinterpret_cast<const unsigned char *>(password.data()), password.size(),
			nullptr, nullptr), srp_user_delete);
	if (!user)
		throw std::runtime_error("SRP user initialization failed");
	unsigned char *a = nullptr;
	size_t a_length = 0;
	if (srp_user_start_authentication(user.get(), nullptr, nullptr, 0,
			&a, &a_length) != SRP_OK)
		throw std::runtime_error("SRP client public key failed");
	unsigned char *b = nullptr;
	size_t b_length = 0;
	std::unique_ptr<SRPVerifier, decltype(&srp_verifier_delete)> server(
		srp_verifier_new(SRP_SHA256, SRP_NG_2048, name,
			reinterpret_cast<const unsigned char *>(salt.data()), salt.size(),
			reinterpret_cast<const unsigned char *>(verifier.data()), verifier.size(),
			a, a_length, nullptr, 0, &b, &b_length, nullptr, nullptr), srp_verifier_delete);
	if (!server || !b)
		throw std::runtime_error("SRP server challenge failed");
	unsigned char *proof = nullptr;
	size_t proof_length = 0;
	srp_user_process_challenge(user.get(),
		reinterpret_cast<const unsigned char *>(salt.data()), salt.size(),
		b, b_length, &proof, &proof_length);
	if (!proof || proof_length != 32)
		throw std::runtime_error("SRP client proof failed");
	std::vector<unsigned char> bad_proof(proof, proof + proof_length);
	bad_proof[0] ^= 1;
	unsigned char *server_proof = nullptr;
	srp_verifier_verify_session(server.get(), bad_proof.data(), &server_proof);
	if (server_proof || srp_verifier_is_authenticated(server.get()))
		throw std::runtime_error("SRP accepted an invalid proof");
	srp_verifier_verify_session(server.get(), proof, &server_proof);
	if (!server_proof || !srp_verifier_is_authenticated(server.get()))
		throw std::runtime_error("SRP server verification failed");
	srp_user_verify_session(user.get(), server_proof);
	if (!srp_user_is_authenticated(user.get()) ||
			std::memcmp(srp_user_get_session_key(user.get(), nullptr),
				srp_verifier_get_session_key(server.get(), nullptr), 32))
		throw std::runtime_error("SRP mutual authentication failed");
	std::fprintf(report, "PASS SRP-2048 mutual authentication, matching session keys and invalid-proof rejection\n");
}

int vitaSelfTest(unsigned short echo_port)
{
	FILE *report = std::fopen("ux0:/data/luanti/vita-selftest.log", "w");
	if (!report)
		return 1;
	std::setvbuf(report, nullptr, _IONBF, 0);
	try {
		const std::wstring unicode = L"Vita \U0001f600 \u00e9";
		auto utf16 = vita_text::encode(unicode, 2048);
		if (vita_text::decode(utf16.data(), utf16.size()) != unicode ||
				vita_text::encode(L"\U0001f600", 1).size() != 1)
			throw std::runtime_error("native keyboard Unicode conversion failed");
		std::fprintf(report, "PASS UTF-16 keyboard roundtrip and surrogate boundary\n");
		MyEventReceiver receiver;
		receiver.reloadKeybindings(KeyPress("SYSTEM_SCANCODE_41"));
		const std::pair<GamepadButton, GameKeyType> bindings[] = {
			{GamepadButton::SOUTH, KeyType::JUMP},
			{GamepadButton::EAST, KeyType::SNEAK},
			{GamepadButton::WEST, KeyType::AUX1},
			{GamepadButton::NORTH, KeyType::INVENTORY},
			{GamepadButton::BACK, KeyType::CHAT},
			{GamepadButton::START, KeyType::ESC},
			{GamepadButton::LEFT_SHOULDER, KeyType::PLACE},
			{GamepadButton::RIGHT_SHOULDER, KeyType::DIG},
			{GamepadButton::DPAD_UP, KeyType::ZOOM},
			{GamepadButton::DPAD_DOWN, KeyType::DROP},
			{GamepadButton::DPAD_LEFT, KeyType::HOTBAR_PREV},
			{GamepadButton::DPAD_RIGHT, KeyType::HOTBAR_NEXT},
		};
		for (auto [button, action] : bindings) {
			receiver.clearInput();
			SEvent event{};
			event.EventType = EET_GAMEPAD_BUTTON_EVENT;
			event.GamepadButtonEvent.Button = button;
			event.GamepadButtonEvent.PressedDown = true;
			receiver.OnEvent(event);
			if (!receiver.IsKeyDown(action) || !receiver.WasKeyPressed(action))
				throw std::runtime_error("controller action missing");
			event.GamepadButtonEvent.PressedDown = false;
			receiver.OnEvent(event);
			if (receiver.IsKeyDown(action) || !receiver.WasKeyReleased(action))
				throw std::runtime_error("controller action stuck");
		}
		std::fprintf(report, "PASS 12 gameplay button mappings, presses and releases\n");
		const std::pair<GamepadAxis, std::pair<GameKeyType, GameKeyType>> axes[] = {
			{GamepadAxis::LEFTX, {KeyType::LEFT, KeyType::RIGHT}},
			{GamepadAxis::LEFTY, {KeyType::FORWARD, KeyType::BACKWARD}},
			{GamepadAxis::RIGHTX, {KeyType::CAMERA_YAW_LEFT, KeyType::CAMERA_YAW_RIGHT}},
			{GamepadAxis::RIGHTY, {KeyType::CAMERA_PITCH_UP, KeyType::CAMERA_PITCH_DOWN}},
		};
		for (auto [axis, actions] : axes) {
			receiver.clearInput();
			SEvent event{};
			event.EventType = EET_GAMEPAD_AXIS_EVENT;
			event.GamepadAxisEvent.Axis = axis;
			for (s16 value : {s16(-24000), s16(24000), s16(0)}) {
				event.GamepadAxisEvent.Value = value;
				receiver.OnEvent(event);
				if (receiver.IsKeyDown(actions.first) != (value < 0) ||
						receiver.IsKeyDown(actions.second) != (value > 0))
					throw std::runtime_error("analog direction or neutral release failed");
			}
		}
		std::fprintf(report, "PASS four analog axes, direction reversal and neutral release\n");
		const std::string dir = "ux0:/data/luanti/selftest";
		const std::string value("binary\0value", 12);
		{
			ModStorageDatabaseSQLite3 db(dir);
			db.beginSave();
			if (!db.setModEntry("test", "key", value))
				throw std::runtime_error("database write failed");
			db.endSave();
		}
		{
			ModStorageDatabaseSQLite3 db(dir);
			std::string stored;
			if (!db.getModEntry("test", "key", &stored) || stored != value)
				throw std::runtime_error("database persistence failed");
			db.beginSave();
			if (!db.removeModEntry("test", "key"))
				throw std::runtime_error("database delete failed");
			db.endSave();
		}
		std::fprintf(report, "PASS mounted SQLite transaction, binary value, reopen and delete\n");
		unsigned char random_a[64], random_b[64];
		if (!secure_rand_fill_buf(random_a, sizeof(random_a)) ||
				!secure_rand_fill_buf(random_b, sizeof(random_b)) ||
				!std::memcmp(random_a, random_b, sizeof(random_a)))
			throw std::runtime_error("kernel random source failed");
		std::fprintf(report, "PASS kernel random source\n");
		testSrp(report);
		UDPSocket unsupported;
		if (unsupported.init(true, true))
			throw std::runtime_error("IPv6 was not rejected");
		std::fprintf(report, "PASS IPv6 rejection\n");
		Semaphore empty;
		if (empty.wait(0) || empty.wait(10) || empty.wait(10))
			throw std::runtime_error("empty semaphore reported a token");
		empty.post(2);
		if (!empty.wait(0) || !empty.wait(10) || empty.wait(0))
			throw std::runtime_error("semaphore token accounting failed");
		std::fprintf(report, "PASS semaphore poll, repeated timeout and token accounting\n");
		if (echo_port) {
			UDPSocket udp(false);
			udp.Bind(Address(0, 0, 0, 0, 0));
			udp.setTimeoutMs(5000);
			Address destination;
			destination.Resolve("127.0.0.1");
			destination.setPort(echo_port);
			udp.Send(destination, value.data(), value.size());
			Address sender;
			char reply[64];
			int received = udp.Receive(sender, reply, sizeof(reply));
			if (received != (int)value.size() || sender != destination ||
					std::memcmp(reply, value.data(), value.size()))
				throw std::runtime_error("host UDP roundtrip failed");
			if (udp.WaitData(30))
				throw std::runtime_error("empty socket timeout failed");
			std::fprintf(report, "PASS numeric resolution, host UDP bind/send/receive and poll timeout\n");
		}
		// Exercise Luanti's reliable UDP peer protocol without a renderer.
		std::unique_ptr<con::IConnection> server(con::createMTP(5.f, false, nullptr));
		std::unique_ptr<con::IConnection> client(con::createMTP(5.f, false, nullptr));
		server->Serve(Address(127, 0, 0, 1, 30997));
		client->Connect(Address(127, 0, 0, 1, 30997));
		NetworkPacket packet;
		NetworkPacket ignored;
		auto receive = [](con::IConnection &connection, NetworkPacket &packet) {
			// NetworkPacket requires clear() before receiving another packet.
			packet.clear();
			return connection.ReceiveTimeoutMs(&packet, 10);
		};
		u64 deadline = getTimeMs() + 5000;
		while (!client->Connected() && getTimeMs() < deadline) {
			receive(*server, packet);
			receive(*client, ignored);
		}
		if (!client->Connected())
			throw std::runtime_error("reliable UDP peer handshake failed");
		// Larger than a datagram: validates fragmentation and reassembly too.
		const std::string large(8192, 'V');
		NetworkPacket outgoing(0x1234, large.size());
		outgoing.putRawString(large);
		client->Send(1, 0, &outgoing, true);
		bool delivered = false;
		deadline = getTimeMs() + 5000;
		while (!delivered && getTimeMs() < deadline) {
			delivered = receive(*server, packet);
			receive(*client, ignored);
		}
		if (!delivered || packet.getCommand() != 0x1234 ||
				packet.getSize() != large.size() || packet.readRawString(large.size()) != large)
			throw std::runtime_error("reliable UDP fragmentation failed");
		session_t peer = packet.getPeerId();
		NetworkPacket response(0x4321, value.size());
		response.putRawString(value);
		server->Send(peer, 1, &response, true);
		delivered = false;
		deadline = getTimeMs() + 5000;
		while (!delivered && getTimeMs() < deadline) {
			delivered = receive(*client, packet);
			receive(*server, ignored);
		}
		if (!delivered || packet.getCommand() != 0x4321 ||
				packet.getSize() != value.size() || packet.readRawString(value.size()) != value)
			throw std::runtime_error("reliable UDP response failed");
		client->Disconnect();
		std::fprintf(report, "PASS reliable UDP handshake, 8 KiB fragmentation and bidirectional channels\n");

		std::fprintf(report, "PASS all requested diagnostics\n");
		std::fclose(report);
		return 0;
	} catch (const std::exception &e) {
		std::fprintf(report, "FAIL %s\n", e.what());
		std::fclose(report);
		return 1;
	}
}
}
