// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/RemoteEngineConnection.h"
#include "robotick/api.h"
#include "robotick/platform/Threading.h"

#include <catch2/catch_all.hpp>
#include <chrono>
#include <string>
#include <vector>

using namespace robotick;

TEST_CASE("Integration/Framework/Data/RemoteEngineConnection (threadless)")
{
	SECTION("Handshake and tick exchange", "[RemoteEngineConnection]")
	{
		static constexpr int target_value = 42;
		int recv_value = 0;
		int send_value = target_value;

		RemoteEngineConnection receiver;
		RemoteEngineConnection sender;

		receiver.configure_receiver("test-receiver");
		receiver.set_field_binder(
			[&](const char* path, RemoteEngineConnection::Field& out)
			{
				if (strcmp(path, "x") == 0)
				{
					out.path = path;
					out.recv_ptr = &recv_value;
					out.size = sizeof(int);
					out.type_desc = TypeRegistry::get().find_by_name("int");
					return true;
				}
				return false;
			});

		sender.configure_sender("test-sender", "test-receiver");
		sender.register_field({"x", &send_value, nullptr, sizeof(int), 0});

		for (int i = 0; i < 50; ++i)
		{
			sender.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			receiver.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(20);
			if (recv_value == target_value)
				break;
		}

		REQUIRE(recv_value == target_value);
	}

	SECTION("Handles large payload", "[RemoteEngineConnection]")
	{
		constexpr uint8_t target_value = 0xAB;
		std::vector<uint8_t> send_buffer(32768, target_value);
		std::vector<uint8_t> receive_buffer(32768);

		RemoteEngineConnection receiver;
		RemoteEngineConnection sender;

		receiver.configure_receiver("test-receiver");
		receiver.set_field_binder(
			[&](const char*, RemoteEngineConnection::Field& out)
			{
				out.recv_ptr = receive_buffer.data();
				out.size = receive_buffer.size();
				out.path = "blob";
				return true;
			});

		sender.configure_sender("test-sender", "test-receiver");
		sender.register_field({"blob", send_buffer.data(), nullptr, send_buffer.size(), 0});

		for (int i = 0; i < 50; ++i)
		{
			receiver.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			sender.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(2);
			if (receive_buffer[100] == target_value)
				break;
		}

		REQUIRE(receive_buffer[100] == target_value);
	}

	SECTION("Reconnect after sender drop", "[RemoteEngineConnection]")
	{
		static constexpr int target_value = 100;
		static constexpr int target_value_later = 200;
		int recv_value = 0;
		int send_value = target_value;

		RemoteEngineConnection rx;
		RemoteEngineConnection tx;

		rx.configure_receiver("test-receiver");
		rx.set_field_binder(
			[&](const char* path, RemoteEngineConnection::Field& out)
			{
				if (strcmp(path, "x") == 0)
				{
					out.path = path;
					out.recv_ptr = &recv_value;
					out.size = sizeof(int);
					out.type_desc = TypeRegistry::get().find_by_name("int");
					return true;
				}
				return false;
			});

		tx.configure_sender("test-sender", "test-receiver");
		tx.register_field({"x", &send_value, nullptr, sizeof(int), 0});

		for (int i = 0; i < 50; ++i)
		{
			rx.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			tx.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(20);
			if (recv_value == target_value)
				break;
		}

		REQUIRE(recv_value == target_value);

		ROBOTICK_INFO("Disconnecting...");

		tx.disconnect();
		Thread::sleep_ms(50);

		// Reconnect
		send_value = target_value_later;
		tx.tick(robotick::TICK_INFO_FIRST_100MS_10HZ); // allow reconnect

		for (int i = 0; i < 50; ++i)
		{
			rx.tick(robotick::TICK_INFO_FIRST_100MS_10HZ);
			tx.tick(robotick::TICK_INFO_FIRST_100MS_10HZ);
			Thread::sleep_ms(20);
			if (recv_value == target_value_later)
				break;
		}

		REQUIRE(recv_value == target_value_later);
	}

	SECTION("Field updates on same connection", "[RemoteEngineConnection]")
	{
		int recv_value = 0;
		int send_value = 11;

		RemoteEngineConnection rx;
		RemoteEngineConnection tx;

		rx.configure_receiver("test-receiver");
		rx.set_field_binder(
			[&](const char*, RemoteEngineConnection::Field& f)
			{
				f.recv_ptr = &recv_value;
				f.size = sizeof(int);
				f.path = "x";
				f.type_desc = TypeRegistry::get().find_by_name("int");
				return true;
			});

		tx.configure_sender("test-sender", "test-receiver");
		tx.register_field({"x", &send_value, nullptr, sizeof(int), 0});

		for (int i = 0; i < 200 && recv_value != 11; ++i)
		{
			rx.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
			tx.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
			Thread::sleep_ms(1);
		}

		REQUIRE(recv_value == 11);

		recv_value = 0;
		send_value = 22;

		for (int i = 0; i < 200 && recv_value != 22; ++i)
		{
			rx.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
			tx.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
			Thread::sleep_ms(1);
		}

		REQUIRE(recv_value == 22);
	}
}
