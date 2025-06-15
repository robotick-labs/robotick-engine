// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/RemoteEngineConnection.h"
#include "robotick/api.h"
#include "robotick/platform/Threading.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <thread>

using namespace robotick;

// Utility for test isolation
static constexpr int BASE_PORT = 34567;
static int next_port()
{
	static std::atomic<int> p{BASE_PORT};
	return p++;
}

TEST_CASE("Integration|Framework|Data|RemoteEngineConnection|Handshake and tick exchange", "[RemoteEngineConnection]")
{
	const int port = next_port();
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
				receiver.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
				Thread::sleep_ms(10);
			}

			while (recv_value != target_value && receiver.is_ready())
			{
				receiver.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
				Thread::sleep_ms(10);
			}
			receiver.disconnect();
		});

	std::thread sender_thread(
		[&]()
		{
			while (!receiver_ready)
				Thread::sleep_ms(5);
			RemoteEngineConnection sender;
			sender.configure({"127.0.0.1", port}, RemoteEngineConnection::Mode::Sender);
			sender.register_field({.path = "x", .send_ptr = &send_value, .size = sizeof(int), .type_hash = 0});
			while (!sender.is_ready())
			{
				sender.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
				Thread::sleep_ms(10);
			}

			sender.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(100);
			sender.disconnect();
		});

	receiver_thread.join();
	sender_thread.join();

	REQUIRE(recv_value == 42);
}

TEST_CASE("Integration|Framework|Data|RemoteEngineConnection|Handles large payload", "[RemoteEngineConnection]")
{
	constexpr uint8_t target_value = 0xAB;
	const int port = next_port();
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
				receiver.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
				Thread::sleep_ms(10);
			}

			while (receive_buffer[100] != target_value && receiver.is_ready())
			{
				receiver.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
				Thread::sleep_ms(10);
			}
			REQUIRE(receive_buffer[100] == 0xAB);
		});

	std::thread sender_thread(
		[&]()
		{
			while (!ready)
				Thread::sleep_ms(5);
			RemoteEngineConnection sender;
			sender.configure({"127.0.0.1", port}, RemoteEngineConnection::Mode::Sender);
			sender.register_field({.path = "blob", .send_ptr = send_buffer.data(), .size = send_buffer.size(), .type_hash = 0});
			while (!sender.is_ready())
			{
				sender.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
				Thread::sleep_ms(10);
			}

			for (int i = 0; i < 3; ++i)
			{
				sender.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
				Thread::sleep_ms(10);
			}
		});

	receiver_thread.join();
	sender_thread.join();
}

// Additional coverage tests

TEST_CASE("Integration|RemoteEngineConnection|Reconnect after sender drop", "[RemoteEngineConnection]")
{
	const int port = next_port();
	std::atomic<bool> receiver_ready{false};

	int recv_value = 0;
	int send_value = 100;

	std::thread receiver_thread(
		[&]()
		{
			RemoteEngineConnection rx;
			rx.configure({"127.0.0.1", port}, RemoteEngineConnection::Mode::Receiver);
			rx.set_field_binder(
				[&](auto const&, RemoteEngineConnection::Field& f)
				{
					f.path = "x";
					f.recv_ptr = &recv_value;
					f.size = sizeof(int);
					f.type_hash = 0;
					return true;
				});
			receiver_ready = true;
			while (!rx.is_ready())
			{
				rx.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
				Thread::sleep_ms(1);
			}
			while (recv_value != 100)
			{
				rx.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
				Thread::sleep_ms(1);
			}

			while (recv_value != 200)
			{
				rx.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
				Thread::sleep_ms(1);
			}
			rx.disconnect();
		});

	std::thread sender_thread(
		[&]()
		{
			while (!receiver_ready)
				Thread::sleep_ms(5);

			RemoteEngineConnection tx;
			tx.configure({"127.0.0.1", port}, RemoteEngineConnection::Mode::Sender);
			tx.register_field({.path = "x", .send_ptr = &send_value, .size = sizeof(int), .type_hash = 0});
			while (!tx.is_ready())
			{
				tx.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
				Thread::sleep_ms(1);
			}
			tx.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(10);
			tx.disconnect();

			send_value = 200;
			while (!tx.is_ready())
			{
				tx.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
				Thread::sleep_ms(1);
			}
			tx.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
			tx.disconnect();
		});

	receiver_thread.join();
	sender_thread.join();
	REQUIRE(recv_value == 200);
}

TEST_CASE("Integration|RemoteEngineConnection|Field updates on same connection", "[RemoteEngineConnection]")
{
	const int port = next_port();
	std::atomic<bool> ready{false};
	int recv_value = 0;
	int send_value = 11;

	std::thread receiver(
		[&]()
		{
			RemoteEngineConnection rx;
			rx.configure({"127.0.0.1", port}, RemoteEngineConnection::Mode::Receiver);
			rx.set_field_binder(
				[&](auto const&, RemoteEngineConnection::Field& f)
				{
					f.recv_ptr = &recv_value;
					f.size = sizeof(int);
					f.path = "x";
					f.type_hash = 0;
					return true;
				});
			ready = true;
			while (!rx.is_ready())
			{
				rx.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
				Thread::sleep_ms(1);
			}
			while (recv_value != 11)
			{
				rx.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
				Thread::sleep_ms(1);
			}

			recv_value = 0;
			while (recv_value != 22)
			{
				rx.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
				Thread::sleep_ms(1);
			}
		});

	std::thread sender(
		[&]()
		{
			while (!ready)
				Thread::sleep_ms(5);
			RemoteEngineConnection tx;
			tx.configure({"127.0.0.1", port}, RemoteEngineConnection::Mode::Sender);
			tx.register_field({.path = "x", .send_ptr = &send_value, .size = sizeof(int), .type_hash = 0});
			while (!tx.is_ready())
			{
				tx.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
				Thread::sleep_ms(1);
			}
			tx.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);

			send_value = 22;
			tx.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
			tx.disconnect();
		});

	receiver.join();
	sender.join();
	REQUIRE(recv_value == 22);
}
