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

	SECTION("Two peers exchange data mutually", "[RemoteEngineConnection]")
	{
		int a_send = 101, a_recv = 0;
		int b_send = 202, b_recv = 0;

		RemoteEngineConnection a_rx, a_tx;
		RemoteEngineConnection b_rx, b_tx;

		// Peer A
		a_rx.configure_receiver("peer-a");
		a_rx.set_field_binder(
			[&](const char* path, RemoteEngineConnection::Field& f)
			{
				if (strcmp(path, "value") == 0)
				{
					f.path = path;
					f.recv_ptr = &a_recv;
					f.size = sizeof(int);
					f.type_desc = TypeRegistry::get().find_by_name("int");
					return true;
				}
				return false;
			});
		a_tx.configure_sender("peer-a", "peer-b");
		a_tx.register_field({"value", &a_send, nullptr, sizeof(int), 0});

		// Peer B
		b_rx.configure_receiver("peer-b");
		b_rx.set_field_binder(
			[&](const char* path, RemoteEngineConnection::Field& f)
			{
				if (strcmp(path, "value") == 0)
				{
					f.path = path;
					f.recv_ptr = &b_recv;
					f.size = sizeof(int);
					f.type_desc = TypeRegistry::get().find_by_name("int");
					return true;
				}
				return false;
			});
		b_tx.configure_sender("peer-b", "peer-a");
		b_tx.register_field({"value", &b_send, nullptr, sizeof(int), 0});

		for (int i = 0; i < 100; ++i)
		{
			a_rx.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			a_tx.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			b_rx.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			b_tx.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(10);
			if (a_recv == b_send && b_recv == a_send)
				break;
		}

		REQUIRE(a_recv == b_send);
		REQUIRE(b_recv == a_send);
	}

	SECTION("Three peers exchange data in a directed ring", "[RemoteEngineConnection]")
	{
		struct Peer
		{
			const char* id;
			int send_value = 0;
			int recv_value = 0;

			RemoteEngineConnection rx;
			RemoteEngineConnection tx;
		};

		Peer a{}, b{}, c{};
		a.id = "peer-a";
		b.id = "peer-b";
		c.id = "peer-c";

		// Assign unique values
		a.send_value = 111;
		b.send_value = 222;
		c.send_value = 333;

		// Configure receivers
		auto setup_receiver = [](Peer& self, const char* expected_sender_id)
		{
			self.rx.configure_receiver(self.id);
			self.rx.set_field_binder(
				[&self, expected_sender_id](const char* path, RemoteEngineConnection::Field& f)
				{
					if (strcmp(path, expected_sender_id) == 0)
					{
						f.path = path;
						f.recv_ptr = (void*)&self.recv_value;
						f.size = sizeof(int);
						f.type_desc = TypeRegistry::get().find_by_name("int");
						return true;
					}
					return false;
				});
		};

		// Configure senders
		auto setup_sender = [](RemoteEngineConnection& tx, const char* from_id, const char* to_id, const char* field_path, int* value_ptr)
		{
			tx.configure_sender(from_id, to_id);
			tx.register_field({field_path, value_ptr, nullptr, sizeof(int), 0});
		};

		// Ring: A → B → C → A
		setup_receiver(a, "peer-c"); // A receives from C
		setup_receiver(b, "peer-a"); // B receives from A
		setup_receiver(c, "peer-b"); // C receives from B

		setup_sender(a.tx, "peer-a", "peer-b", "peer-a", &a.send_value);
		setup_sender(b.tx, "peer-b", "peer-c", "peer-b", &b.send_value);
		setup_sender(c.tx, "peer-c", "peer-a", "peer-c", &c.send_value);

		// Tick until all values arrive
		for (int i = 0; i < 100; ++i)
		{
			a.rx.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			a.tx.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);

			b.rx.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			b.tx.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);

			c.rx.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			c.tx.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);

			Thread::sleep_ms(10);

			if (a.recv_value == c.send_value && b.recv_value == a.send_value && c.recv_value == b.send_value)
			{
				break;
			}
		}

		ROBOTICK_INFO("A received: %d (from C)", a.recv_value);
		ROBOTICK_INFO("B received: %d (from A)", b.recv_value);
		ROBOTICK_INFO("C received: %d (from B)", c.recv_value);

		REQUIRE(a.recv_value == c.send_value);
		REQUIRE(b.recv_value == a.send_value);
		REQUIRE(c.recv_value == b.send_value);
	}

	SECTION("Directed triangle: A → B, A → C, C → A", "[RemoteEngineConnection]")
	{
		struct Peer
		{
			const char* id;
			int recv_value = 0;
			int send_value = 0;
			RemoteEngineConnection rx;
			RemoteEngineConnection tx;
		};

		Peer a{}, b{}, c{};
		a.id = "peer-a";
		b.id = "peer-b";
		c.id = "peer-c";

		a.send_value = 111;
		c.send_value = 333;

		// Configure receivers
		a.rx.configure_receiver("peer-a");
		a.rx.set_field_binder(
			[&](const char* path, RemoteEngineConnection::Field& f)
			{
				if (strcmp(path, "peer-c") == 0)
				{
					f.path = path;
					f.recv_ptr = &a.recv_value;
					f.size = sizeof(int);
					f.type_desc = TypeRegistry::get().find_by_name("int");
					return true;
				}
				return false;
			});

		b.rx.configure_receiver("peer-b");
		b.rx.set_field_binder(
			[&](const char* path, RemoteEngineConnection::Field& f)
			{
				if (strcmp(path, "peer-a") == 0)
				{
					f.path = path;
					f.recv_ptr = &b.recv_value;
					f.size = sizeof(int);
					f.type_desc = TypeRegistry::get().find_by_name("int");
					return true;
				}
				return false;
			});

		c.rx.configure_receiver("peer-c");
		c.rx.set_field_binder(
			[](const char*, RemoteEngineConnection::Field&)
			{
				return false; // C is only a sender in this direction
			});

		// Configure senders
		a.tx.configure_sender("peer-a", "peer-b");
		a.tx.register_field({"peer-a", &a.send_value, nullptr, sizeof(int), 0});

		RemoteEngineConnection a_to_c;
		a_to_c.configure_sender("peer-a", "peer-c");
		a_to_c.register_field({"peer-a", &a.send_value, nullptr, sizeof(int), 0});

		c.tx.configure_sender("peer-c", "peer-a");
		c.tx.register_field({"peer-c", &c.send_value, nullptr, sizeof(int), 0});

		// Tick loop
		for (int i = 0; i < 200; ++i)
		{
			a.rx.tick(TICK_INFO_FIRST_10MS_100HZ);
			b.rx.tick(TICK_INFO_FIRST_10MS_100HZ);
			c.rx.tick(TICK_INFO_FIRST_10MS_100HZ);

			a.tx.tick(TICK_INFO_FIRST_10MS_100HZ);
			a_to_c.tick(TICK_INFO_FIRST_10MS_100HZ);
			c.tx.tick(TICK_INFO_FIRST_10MS_100HZ);

			Thread::sleep_ms(10);

			if (b.recv_value == a.send_value && a.recv_value == c.send_value)
				break;
		}

		ROBOTICK_INFO("B received: %d (from A)", b.recv_value);
		ROBOTICK_INFO("C received: N/A"); // No receiver
		ROBOTICK_INFO("A received: %d (from C)", a.recv_value);

		REQUIRE(b.recv_value == a.send_value);
		REQUIRE(a.recv_value == c.send_value);
	}

	SECTION("Multiple peers send to one (D→A, C→A, B→A)", "[RemoteEngineConnection]")
	{
		struct Sender
		{
			const char* id;
			int value = 0;
			RemoteEngineConnection tx;
		};

		struct Receiver
		{
			const char* id;
			int recv_from_b = 0;
			int recv_from_c = 0;
			int recv_from_d = 0;

			RemoteEngineConnection rx_b;
			RemoteEngineConnection rx_c;
			RemoteEngineConnection rx_d;
		};

		Sender b, c, d;
		b.id = "peer-b";
		b.value = 111;

		c.id = "peer-c";
		c.value = 222;

		d.id = "peer-d";
		d.value = 333;

		Receiver a;
		a.id = "peer-a";

		// Configure A's receivers
		auto setup_receiver = [](RemoteEngineConnection& rx, const char* self_id, const char* expected_sender, int* target)
		{
			rx.configure_receiver(self_id);
			rx.set_field_binder(
				[=](const char* path, RemoteEngineConnection::Field& f)
				{
					if (strcmp(path, expected_sender) == 0)
					{
						f.path = path;
						f.recv_ptr = target;
						f.size = sizeof(int);
						f.type_desc = TypeRegistry::get().find_by_name("int");
						return true;
					}
					return false;
				});
		};

		setup_receiver(a.rx_b, "peer-a", "peer-b", &a.recv_from_b);
		setup_receiver(a.rx_c, "peer-a", "peer-c", &a.recv_from_c);
		setup_receiver(a.rx_d, "peer-a", "peer-d", &a.recv_from_d);

		// Configure each sender
		auto setup_sender = [](RemoteEngineConnection& tx, const char* from, const char* to, const char* path, int* value_ptr)
		{
			tx.configure_sender(from, to);
			tx.register_field({path, value_ptr, nullptr, sizeof(int), 0});
		};

		setup_sender(b.tx, "peer-b", "peer-a", "peer-b", &b.value);
		setup_sender(c.tx, "peer-c", "peer-a", "peer-c", &c.value);
		setup_sender(d.tx, "peer-d", "peer-a", "peer-d", &d.value);

		// Tick loop
		for (int i = 0; i < 200; ++i)
		{
			a.rx_b.tick(TICK_INFO_FIRST_10MS_100HZ);
			a.rx_c.tick(TICK_INFO_FIRST_10MS_100HZ);
			a.rx_d.tick(TICK_INFO_FIRST_10MS_100HZ);

			b.tx.tick(TICK_INFO_FIRST_10MS_100HZ);
			c.tx.tick(TICK_INFO_FIRST_10MS_100HZ);
			d.tx.tick(TICK_INFO_FIRST_10MS_100HZ);

			Thread::sleep_ms(10);

			if (a.recv_from_b == b.value && a.recv_from_c == c.value && a.recv_from_d == d.value)
				break;
		}

		ROBOTICK_INFO("A received from B: %d", a.recv_from_b);
		ROBOTICK_INFO("A received from C: %d", a.recv_from_c);
		ROBOTICK_INFO("A received from D: %d", a.recv_from_d);

		REQUIRE(a.recv_from_b == b.value);
		REQUIRE(a.recv_from_c == c.value);
		REQUIRE(a.recv_from_d == d.value);
	}
}
