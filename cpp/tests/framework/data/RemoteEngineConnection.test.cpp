// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/RemoteEngineConnection.h"
#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <thread>

using namespace robotick;

TEST_CASE("RemoteEngineConnection|Handshake and tick exchange", "[RemoteEngineConnection]")
{
	constexpr int port = 34567;
	std::atomic<bool> server_ready{false};
	int recv_value = 0;
	int send_value = 42;

	std::thread server(
		[&]()
		{
			RemoteEngineConnection server({"127.0.0.1", port}, RemoteEngineConnection::Mode::Receiver);
			server.set_field_binder(
				[&](const std::string& path, RemoteEngineConnection::Field& out)
				{
					if (path == "x")
					{
						out.path = path;
						out.recv_ptr = &recv_value;
						out.size = sizeof(int);
						out.type_hash = 0;
						return true;
					}
					return false;
				});
			server_ready = true;
			while (!server.is_ready_for_tick())
				server.tick();
			for (int i = 0; i < 3; ++i)
				server.tick();
		});

	std::thread client(
		[&]()
		{
			while (!server_ready)
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			RemoteEngineConnection client({"127.0.0.1", port}, RemoteEngineConnection::Mode::Sender);
			client.register_field({.path = "x", .send_ptr = &send_value, .size = sizeof(int), .type_hash = 0});
			while (!client.is_ready_for_tick())
				client.tick();
			for (int i = 0; i < 3; ++i)
				client.tick();
		});

	server.join();
	client.join();
	REQUIRE(recv_value == 42);
}

TEST_CASE("RemoteEngineConnection|Handles large payload", "[RemoteEngineConnection]")
{
	constexpr int port = 34568;
	std::atomic<bool> ready{false};
	std::vector<uint8_t> buffer(32768, 0xAB);

	std::thread server(
		[&]()
		{
			RemoteEngineConnection server({"127.0.0.1", port}, RemoteEngineConnection::Mode::Receiver);
			std::vector<uint8_t> recv(32768);
			server.set_field_binder(
				[&](const std::string&, RemoteEngineConnection::Field& out)
				{
					out.recv_ptr = recv.data();
					out.size = recv.size();
					out.path = "blob";
					return true;
				});
			ready = true;
			while (!server.is_ready_for_tick())
				server.tick();
			for (int i = 0; i < 3; ++i)
				server.tick();
			REQUIRE(recv[100] == 0xAB);
		});

	std::thread client(
		[&]()
		{
			while (!ready)
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			RemoteEngineConnection client({"127.0.0.1", port}, RemoteEngineConnection::Mode::Sender);
			client.register_field({.path = "blob", .send_ptr = buffer.data(), .size = buffer.size(), .type_hash = 0});
			while (!client.is_ready_for_tick())
				client.tick();
			for (int i = 0; i < 3; ++i)
				client.tick();
		});

	server.join();
	client.join();
}
