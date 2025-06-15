// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/RemoteEngineConnection.h"
#include "robotick/platform/Threading.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <thread>

using namespace robotick;

TEST_CASE("Integration|Framework|Data|RemoteEngineConnection|Handshake and tick exchange", "[RemoteEngineConnection]")
{
	constexpr int port = 34567;
	std::atomic<bool> receiver_ready{false};

	static constexpr int target_value = 42;

	int recv_value = 0;
	int send_value = target_value;

	std::thread receiver_thread(
		[&]()
		{
			RemoteEngineConnection receiver;
			receiver.configure({"127.0.0.1", port}, RemoteEngineConnection::Mode::Receiver);
			receiver.set_field_binder(
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
			receiver_ready = true;
			while (!receiver.is_ready())
			{
				receiver.tick();
				Thread::sleep_ms(10);
			}

			while (recv_value != target_value && receiver.is_ready())
			{
				receiver.tick();
				Thread::sleep_ms(10);
			}

			receiver.disconnect(); // does happen in destructor - but handy for debugging to do it explicitly here
		});

	std::thread sender_thread(
		[&]()
		{
			while (!receiver_ready)
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			RemoteEngineConnection sender;
			sender.configure({"127.0.0.1", port}, RemoteEngineConnection::Mode::Sender);
			sender.register_field({.path = "x", .send_ptr = &send_value, .size = sizeof(int), .type_hash = 0});
			while (!sender.is_ready())
			{
				sender.tick();
				Thread::sleep_ms(10);
			}

			sender.tick(); // send a single fields-packet now that receiver is ready

			int wait_count = 0;
			while (recv_value != 42 && wait_count < 10) // wait for up to 1 second for recv_value to change
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				wait_count++;
			}

			sender.disconnect(); // does happen in destructor - but handy for debugging to do it explicitly here
		});

	receiver_thread.join();
	sender_thread.join();

	REQUIRE(recv_value == 42);
}

TEST_CASE("Integration|Framework|Data|RemoteEngineConnection|Handles large payload", "[RemoteEngineConnection]")
{
	constexpr uint8_t target_value = 0xAB;
	constexpr int port = 34568;
	std::atomic<bool> ready{false};

	std::vector<uint8_t> send_buffer(32768, target_value);
	std::vector<uint8_t> receive_buffer(32768);

	std::thread receiver_thread(
		[&]()
		{
			RemoteEngineConnection receiver;
			receiver.configure({"127.0.0.1", port}, RemoteEngineConnection::Mode::Receiver);
			receiver.set_field_binder(
				[&](const std::string&, RemoteEngineConnection::Field& out)
				{
					out.recv_ptr = receive_buffer.data();
					out.size = receive_buffer.size();
					out.path = "blob";
					return true;
				});
			ready = true;
			while (!receiver.is_ready())
			{
				receiver.tick();
				Thread::sleep_ms(10);
			}

			while (receive_buffer[100] != target_value && receiver.is_ready())
			{
				receiver.tick();
				Thread::sleep_ms(10);
			}

			REQUIRE(receive_buffer[100] == 0xAB);
		});

	std::thread sender_thread(
		[&]()
		{
			while (!ready)
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			RemoteEngineConnection sender;
			sender.configure({"127.0.0.1", port}, RemoteEngineConnection::Mode::Sender);
			sender.register_field({.path = "blob", .send_ptr = send_buffer.data(), .size = send_buffer.size(), .type_hash = 0});
			while (!sender.is_ready())
			{
				sender.tick();
				Thread::sleep_ms(10);
			}

			for (int i = 0; i < 3; ++i)
			{
				sender.tick();
				Thread::sleep_ms(10);
			}
		});

	receiver_thread.join();
	sender_thread.join();
}
