// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/RemoteEngineConnection.h"

#include "robotick/api.h"
#include "robotick/framework/concurrency/Thread.h"
#include "robotick/framework/containers/FixedVector.h"
#include "robotick/framework/containers/HeapVector.h"
#include "robotick/framework/strings/FixedString.h"
#include "robotick/framework/strings/StringUtils.h"

#include <catch2/catch_all.hpp>

using namespace robotick;

static int wait_for_listen_port(RemoteEngineConnection& receiver, int max_attempts = 50, int sleep_ms = 10)
{
	int port = 0;
	for (int i = 0; i < max_attempts; ++i)
	{
		receiver.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
		port = receiver.get_listen_port();
		if (port > 0)
			return port;
		Thread::sleep_ms(sleep_ms);
	}
	FixedString256 msg;
	msg.format("wait_for_listen_port timed out (port=%d attempts=%d sleep_ms=%d)", port, max_attempts, sleep_ms);
	WARN(msg.c_str());
	return port; // 0 if failed
}

TEST_CASE("Integration/Framework/Data/RemoteEngineConnection")
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
				if (string_equals(path, "x"))
				{
					out.path = path;
					out.recv_ptr = &recv_value;
					out.size = sizeof(int);
					out.type_desc = TypeRegistry::get().find_by_name("int");
					return true;
				}
				return false;
			});

		const int receiver_listen_port = wait_for_listen_port(receiver);
		REQUIRE(receiver_listen_port > 0);

		sender.configure_sender("test-sender", "test-receiver", "127.0.0.1", receiver_listen_port);
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

	SECTION("Handshake binds last field without trailing newline", "[RemoteEngineConnection]")
	{
		int recv_a = 0;
		int recv_b = 0;
		int send_a = 7;
		int send_b = 9;

		RemoteEngineConnection receiver;
		RemoteEngineConnection sender;

		const char* path_a = "alpha";
		const char* path_b = "beta_final";

		size_t bind_count = 0;

		receiver.configure_receiver("test-receiver");
		receiver.set_field_binder(
			[&](const char* path, RemoteEngineConnection::Field& out)
			{
				if (string_equals(path, path_a))
				{
					out.path = path;
					out.recv_ptr = &recv_a;
					out.size = sizeof(int);
					out.type_desc = TypeRegistry::get().find_by_name("int");
					++bind_count;
					return true;
				}
				if (string_equals(path, path_b))
				{
					out.path = path;
					out.recv_ptr = &recv_b;
					out.size = sizeof(int);
					out.type_desc = TypeRegistry::get().find_by_name("int");
					++bind_count;
					return true;
				}
				return false;
			});

		const int receiver_listen_port = wait_for_listen_port(receiver);
		REQUIRE(receiver_listen_port > 0);

		sender.configure_sender("test-sender", "test-receiver", "127.0.0.1", receiver_listen_port);
		sender.register_field({path_a, &send_a, nullptr, sizeof(int), 0});
		sender.register_field({path_b, &send_b, nullptr, sizeof(int), 0});

		for (int i = 0; i < 50; ++i)
		{
			sender.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			receiver.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(10);

			if (recv_a == send_a && recv_b == send_b)
				break;
		}

		REQUIRE(bind_count == 2); // both fields bound (including last without trailing newline)
		REQUIRE(recv_a == send_a);
		REQUIRE(recv_b == send_b);
	}

	SECTION("Handshake handles near-capacity field path", "[RemoteEngineConnection]")
	{
		const size_t max_path_len = FixedString512().capacity() - 1;
		FixedString512 long_path;
		const size_t fill_len = max_path_len - 1;
		for (size_t i = 0; i < fill_len; ++i)
		{
			long_path.data[i] = 'p';
		}
		long_path.data[fill_len] = '\0';

		int recv_val = 0;
		int send_val = 123;
		bool bound = false;

		RemoteEngineConnection receiver;
		RemoteEngineConnection sender;

		receiver.configure_receiver("rx");
		receiver.set_field_binder(
			[&](const char* path, RemoteEngineConnection::Field& out)
			{
				if (string_equals(path, long_path.c_str()))
				{
					out.path = path;
					out.recv_ptr = &recv_val;
					out.size = sizeof(int);
					out.type_desc = TypeRegistry::get().find_by_name("int");
					bound = true;
					return true;
				}
				return false;
			});

		const int receiver_listen_port = wait_for_listen_port(receiver);
		REQUIRE(receiver_listen_port > 0);

		sender.configure_sender("tx", "rx", "127.0.0.1", receiver_listen_port);
		sender.register_field({long_path.c_str(), &send_val, nullptr, sizeof(int), 0});

		for (int i = 0; i < 50; ++i)
		{
			sender.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			receiver.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(10);
			if (recv_val == send_val)
				break;
		}

		REQUIRE(bound);
		REQUIRE(recv_val == send_val);
	}

	SECTION("Handshake binds multiple fields reliably", "[RemoteEngineConnection]")
	{
		static constexpr int kFieldCount = 6;
		int recv_values[kFieldCount] = {};
		int send_values[kFieldCount] = {1, 2, 3, 4, 5, 6};

		FixedVector<FixedString64, kFieldCount> paths;
		for (int i = 0; i < kFieldCount; ++i)
		{
			FixedString64 name;
			name.format("field_%d", i);
			paths.add(name);
		}

		int bound_count = 0;

		RemoteEngineConnection receiver;
		RemoteEngineConnection sender;

		receiver.configure_receiver("rx");
		receiver.set_field_binder(
			[&](const char* path, RemoteEngineConnection::Field& out)
			{
				for (int i = 0; i < kFieldCount; ++i)
				{
					if (string_equals(paths[i].c_str(), path))
					{
						out.path = path;
						out.recv_ptr = &recv_values[i];
						out.size = sizeof(int);
						out.type_desc = TypeRegistry::get().find_by_name("int");
						++bound_count;
						return true;
					}
				}
				return false;
			});

		const int receiver_listen_port = wait_for_listen_port(receiver);
		REQUIRE(receiver_listen_port > 0);

		sender.configure_sender("tx", "rx", "127.0.0.1", receiver_listen_port);
		for (int i = 0; i < kFieldCount; ++i)
		{
			sender.register_field({paths[i].c_str(), &send_values[i], nullptr, sizeof(int), 0});
		}

		for (int i = 0; i < 100; ++i)
		{
			sender.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			receiver.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(5);

			bool all_match = true;
			for (int j = 0; j < kFieldCount; ++j)
			{
				if (recv_values[j] != send_values[j])
					all_match = false;
			}
			if (all_match)
				break;
		}

		REQUIRE(bound_count == kFieldCount);
		for (int i = 0; i < kFieldCount; ++i)
		{
			REQUIRE(recv_values[i] == send_values[i]);
		}
	}

	SECTION("Handshake grows receiver field storage beyond initial reserve", "[RemoteEngineConnection]")
	{
		static constexpr int kFieldCount = 20;
		int recv_values[kFieldCount] = {};
		int send_values[kFieldCount] = {};
		for (int i = 0; i < kFieldCount; ++i)
		{
			send_values[i] = i + 100;
		}

		FixedVector<FixedString64, kFieldCount> paths;
		for (int i = 0; i < kFieldCount; ++i)
		{
			FixedString64 name;
			name.format("grow_field_%d", i);
			paths.add(name);
		}

		int bound_count = 0;

		RemoteEngineConnection receiver;
		RemoteEngineConnection sender;

		receiver.configure_receiver("rx");
		receiver.set_field_binder(
			[&](const char* path, RemoteEngineConnection::Field& out)
			{
				for (int i = 0; i < kFieldCount; ++i)
				{
					if (string_equals(paths[i].c_str(), path))
					{
						out.path = path;
						out.recv_ptr = &recv_values[i];
						out.size = sizeof(int);
						out.type_desc = TypeRegistry::get().find_by_name("int");
						++bound_count;
						return true;
					}
				}
				return false;
			});

		const int receiver_listen_port = wait_for_listen_port(receiver);
		REQUIRE(receiver_listen_port > 0);

		sender.configure_sender("tx", "rx", "127.0.0.1", receiver_listen_port);
		for (int i = 0; i < kFieldCount; ++i)
		{
			sender.register_field({paths[i].c_str(), &send_values[i], nullptr, sizeof(int), 0});
		}

		for (int i = 0; i < 150; ++i)
		{
			sender.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			receiver.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(5);

			bool all_match = true;
			for (int j = 0; j < kFieldCount; ++j)
			{
				if (recv_values[j] != send_values[j])
				{
					all_match = false;
				}
			}
			if (all_match)
				break;
		}

		REQUIRE(bound_count == kFieldCount);
		for (int i = 0; i < kFieldCount; ++i)
		{
			REQUIRE(recv_values[i] == send_values[i]);
		}
	}

	SECTION("Handles large payload", "[RemoteEngineConnection]")
	{
		constexpr uint8_t target_value = 0xAB;
		HeapVector<uint8_t> send_buffer;
		send_buffer.initialize(32768);
		for (size_t i = 0; i < send_buffer.size(); ++i)
			send_buffer[i] = target_value;
		HeapVector<uint8_t> receive_buffer;
		receive_buffer.initialize(32768);

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

		const int receiver_listen_port = wait_for_listen_port(receiver);
		REQUIRE(receiver_listen_port > 0);

		sender.configure_sender("test-sender", "test-receiver", "127.0.0.1", receiver_listen_port);
		sender.register_field({"blob", send_buffer.data(), nullptr, send_buffer.size(), 0});

		for (int i = 0; i < 50; ++i)
		{
			receiver.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			sender.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(2);
			bool matches = true;
			for (size_t j = 0; j < receive_buffer.size(); ++j)
			{
				if (receive_buffer[j] != send_buffer[j])
				{
					matches = false;
					break;
				}
			}
			if (matches)
				break;
		}

		for (size_t i = 0; i < receive_buffer.size(); ++i)
		{
			REQUIRE(receive_buffer[i] == target_value);
		}
	}

	SECTION("Stress large count and blob updates", "[RemoteEngineConnection]")
	{
		static constexpr size_t kBlobCapacity = 8192;
		static constexpr uint32_t kBaseCount = 6400;
		static constexpr uint32_t kFrameCount = 48;

		uint32_t recv_count = 0;
		uint32_t send_count = 0;
		HeapVector<uint8_t> send_buffer;
		send_buffer.initialize(kBlobCapacity);
		HeapVector<uint8_t> receive_buffer;
		receive_buffer.initialize(kBlobCapacity);

		RemoteEngineConnection receiver;
		RemoteEngineConnection sender;

		receiver.configure_receiver("test-receiver");
		receiver.set_field_binder(
			[&](const char* path, RemoteEngineConnection::Field& out)
			{
				if (string_equals(path, "blob.count"))
				{
					out.recv_ptr = &recv_count;
					out.size = sizeof(recv_count);
					out.path = path;
					out.type_desc = TypeRegistry::get().find_by_name("uint32_t");
					return true;
				}

				if (string_equals(path, "blob.data"))
				{
					out.recv_ptr = receive_buffer.data();
					out.size = receive_buffer.size();
					out.path = path;
					return true;
				}

				return false;
			});

		const int receiver_listen_port = wait_for_listen_port(receiver);
		REQUIRE(receiver_listen_port > 0);

		sender.configure_sender("test-sender", "test-receiver", "127.0.0.1", receiver_listen_port);
		sender.register_field({"blob.count", &send_count, nullptr, sizeof(send_count), 0});
		sender.register_field({"blob.data", send_buffer.data(), nullptr, send_buffer.size(), 0});

		auto buffers_match = [&]() -> bool
		{
			for (size_t i = 0; i < send_buffer.size(); ++i)
			{
				if (receive_buffer[i] != send_buffer[i])
				{
					return false;
				}
			}
			return true;
		};

		for (uint32_t frame = 0; frame < kFrameCount; ++frame)
		{
			send_count = kBaseCount + (frame % 97);
			const uint8_t frame_seed = static_cast<uint8_t>((frame * 37U) & 0xFFU);
			for (size_t i = 0; i < send_buffer.size(); ++i)
			{
				send_buffer[i] = (i < send_count) ? static_cast<uint8_t>(frame_seed + (i % 17U)) : static_cast<uint8_t>(0xE0U + frame);
			}

			bool delivered = false;
			for (int step = 0; step < 100; ++step)
			{
				receiver.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
				sender.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);

				if (recv_count == send_count)
				{
					REQUIRE(buffers_match());
				}

				if (recv_count == send_count && buffers_match())
				{
					delivered = true;
					break;
				}

				Thread::sleep_ms(1);
			}

			REQUIRE(delivered);
			REQUIRE(recv_count == send_count);
			REQUIRE(buffers_match());
		}
	}

	SECTION("Reconnect after sender drop", "[RemoteEngineConnection]")
	{
		static constexpr int target_value = 100;
		static constexpr int target_value_later = 200;
		int recv_value = 0;
		int send_value = target_value;

		RemoteEngineConnection receiver;
		RemoteEngineConnection sender;

		receiver.configure_receiver("test-receiver");
		receiver.set_field_binder(
			[&](const char* path, RemoteEngineConnection::Field& out)
			{
				if (string_equals(path, "x"))
				{
					out.path = path;
					out.recv_ptr = &recv_value;
					out.size = sizeof(int);
					out.type_desc = TypeRegistry::get().find_by_name("int");
					return true;
				}
				return false;
			});

		const int receiver_listen_port = wait_for_listen_port(receiver);
		REQUIRE(receiver_listen_port > 0);

		sender.configure_sender("test-sender", "test-receiver", "127.0.0.1", receiver_listen_port);
		sender.register_field({"x", &send_value, nullptr, sizeof(int), 0});

		for (int i = 0; i < 50; ++i)
		{
			receiver.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			sender.tick(robotick::TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(20);
			if (recv_value == target_value)
				break;
		}

		REQUIRE(recv_value == target_value);

		ROBOTICK_INFO("Disconnecting...");

		sender.disconnect();
		Thread::sleep_ms(50);

		// Reconnect
		send_value = target_value_later;
		sender.tick(robotick::TICK_INFO_FIRST_100MS_10HZ); // allow reconnect

		for (int i = 0; i < 50; ++i)
		{
			receiver.tick(robotick::TICK_INFO_FIRST_100MS_10HZ);
			sender.tick(robotick::TICK_INFO_FIRST_100MS_10HZ);
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

		RemoteEngineConnection receiver;
		RemoteEngineConnection sender;

		receiver.configure_receiver("test-receiver");
		receiver.set_field_binder(
			[&](const char*, RemoteEngineConnection::Field& f)
			{
				f.recv_ptr = &recv_value;
				f.size = sizeof(int);
				f.path = "x";
				f.type_desc = TypeRegistry::get().find_by_name("int");
				return true;
			});

		const int receiver_listen_port = wait_for_listen_port(receiver);
		REQUIRE(receiver_listen_port > 0);

		sender.configure_sender("test-sender", "test-receiver", "127.0.0.1", receiver_listen_port);
		sender.register_field({"x", &send_value, nullptr, sizeof(int), 0});

		for (int i = 0; i < 200 && recv_value != 11; ++i)
		{
			receiver.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
			sender.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
			Thread::sleep_ms(1);
		}

		REQUIRE(recv_value == 11);

		recv_value = 0;
		send_value = 22;

		for (int i = 0; i < 200 && recv_value != 22; ++i)
		{
			receiver.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
			sender.tick(robotick::TICK_INFO_FIRST_1MS_1KHZ);
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
				if (string_equals(path, "value"))
				{
					f.path = path;
					f.recv_ptr = &a_recv;
					f.size = sizeof(int);
					f.type_desc = TypeRegistry::get().find_by_name("int");
					return true;
				}
				return false;
			});

		const int a_rx_listen_port = wait_for_listen_port(a_rx);
		REQUIRE(a_rx_listen_port > 0);

		// Peer B
		b_rx.configure_receiver("peer-b");
		b_rx.set_field_binder(
			[&](const char* path, RemoteEngineConnection::Field& f)
			{
				if (string_equals(path, "value"))
				{
					f.path = path;
					f.recv_ptr = &b_recv;
					f.size = sizeof(int);
					f.type_desc = TypeRegistry::get().find_by_name("int");
					return true;
				}
				return false;
			});

		const int b_rx_listen_port = wait_for_listen_port(b_rx);
		REQUIRE(b_rx_listen_port > 0);

		a_tx.configure_sender("peer-a", "peer-b", "127.0.0.1", b_rx_listen_port);
		a_tx.register_field({"value", &a_send, nullptr, sizeof(int), 0});

		b_tx.configure_sender("peer-b", "peer-a", "127.0.0.1", a_rx_listen_port);
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

			RemoteEngineConnection receiver;
			RemoteEngineConnection sender;
		};

		Peer a, b, c;
		a.id = "peer-a";
		a.send_value = 111;
		b.id = "peer-b";
		b.send_value = 222;
		c.id = "peer-c";
		c.send_value = 333;

		auto setup_receiver = [](Peer& peer, const char* expected_sender_id)
		{
			peer.receiver.configure_receiver(peer.id);
			peer.receiver.set_field_binder(
				[&peer, expected_sender_id](const char* path, RemoteEngineConnection::Field& f)
				{
					if (string_equals(path, expected_sender_id))
					{
						f.path = path;
						f.recv_ptr = &peer.recv_value;
						f.size = sizeof(int);
						f.type_desc = TypeRegistry::get().find_by_name("int");
						return true;
					}
					return false;
				});
		};

		setup_receiver(a, "peer-c");
		setup_receiver(b, "peer-a");
		setup_receiver(c, "peer-b");

		const int port_a = wait_for_listen_port(a.receiver);
		const int port_b = wait_for_listen_port(b.receiver);
		const int port_c = wait_for_listen_port(c.receiver);
		REQUIRE(port_a > 0);
		REQUIRE(port_b > 0);
		REQUIRE(port_c > 0);

		a.sender.configure_sender("peer-a", "peer-b", "127.0.0.1", port_b);
		a.sender.register_field({"peer-a", &a.send_value, nullptr, sizeof(int), 0});

		b.sender.configure_sender("peer-b", "peer-c", "127.0.0.1", port_c);
		b.sender.register_field({"peer-b", &b.send_value, nullptr, sizeof(int), 0});

		c.sender.configure_sender("peer-c", "peer-a", "127.0.0.1", port_a);
		c.sender.register_field({"peer-c", &c.send_value, nullptr, sizeof(int), 0});

		for (int i = 0; i < 100; ++i)
		{
			a.receiver.tick(TICK_INFO_FIRST_10MS_100HZ);
			b.receiver.tick(TICK_INFO_FIRST_10MS_100HZ);
			c.receiver.tick(TICK_INFO_FIRST_10MS_100HZ);

			a.sender.tick(TICK_INFO_FIRST_10MS_100HZ);
			b.sender.tick(TICK_INFO_FIRST_10MS_100HZ);
			c.sender.tick(TICK_INFO_FIRST_10MS_100HZ);

			Thread::sleep_ms(10);

			if (a.recv_value == c.send_value && b.recv_value == a.send_value && c.recv_value == b.send_value)
				break;
		}

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
			RemoteEngineConnection receiver;
			RemoteEngineConnection sender;
		};

		Peer a, b, c;
		a.id = "peer-a";
		a.send_value = 111;
		b.id = "peer-b";
		c.id = "peer-c";
		c.send_value = 333;

		a.receiver.configure_receiver("peer-a");
		a.receiver.set_field_binder(
			[&](const char* path, RemoteEngineConnection::Field& f)
			{
				if (string_equals(path, "peer-c"))
				{
					f.path = path;
					f.recv_ptr = &a.recv_value;
					f.size = sizeof(int);
					f.type_desc = TypeRegistry::get().find_by_name("int");
					return true;
				}
				return false;
			});

		b.receiver.configure_receiver("peer-b");
		b.receiver.set_field_binder(
			[&](const char* path, RemoteEngineConnection::Field& f)
			{
				if (string_equals(path, "peer-a"))
				{
					f.path = path;
					f.recv_ptr = &b.recv_value;
					f.size = sizeof(int);
					f.type_desc = TypeRegistry::get().find_by_name("int");
					return true;
				}
				return false;
			});

		c.receiver.configure_receiver("peer-c");
		c.receiver.set_field_binder(
			[&](const char* path, RemoteEngineConnection::Field& f)
			{
				if (string_equals(path, "peer-a"))
				{
					f.path = path;
					f.recv_ptr = &c.recv_value;
					f.size = sizeof(int);
					f.type_desc = TypeRegistry::get().find_by_name("int");
					return true;
				}
				return false;
			});

		const int port_a = wait_for_listen_port(a.receiver);
		const int port_b = wait_for_listen_port(b.receiver);
		const int port_c = wait_for_listen_port(c.receiver);
		REQUIRE(port_a > 0);
		REQUIRE(port_b > 0);
		REQUIRE(port_c > 0);

		a.sender.configure_sender("peer-a", "peer-b", "127.0.0.1", port_b);
		a.sender.register_field({"peer-a", &a.send_value, nullptr, sizeof(int), 0});

		RemoteEngineConnection a_to_c;
		a_to_c.configure_sender("peer-a", "peer-c", "127.0.0.1", port_c);
		a_to_c.register_field({"peer-a", &a.send_value, nullptr, sizeof(int), 0});

		c.sender.configure_sender("peer-c", "peer-a", "127.0.0.1", port_a);
		c.sender.register_field({"peer-c", &c.send_value, nullptr, sizeof(int), 0});

		for (int i = 0; i < 200; ++i)
		{
			a.receiver.tick(TICK_INFO_FIRST_10MS_100HZ);
			b.receiver.tick(TICK_INFO_FIRST_10MS_100HZ);
			c.receiver.tick(TICK_INFO_FIRST_10MS_100HZ);

			a.sender.tick(TICK_INFO_FIRST_10MS_100HZ);
			a_to_c.tick(TICK_INFO_FIRST_10MS_100HZ);
			c.sender.tick(TICK_INFO_FIRST_10MS_100HZ);

			Thread::sleep_ms(10);

			if (b.recv_value == a.send_value && a.recv_value == c.send_value)
				break;
		}

		REQUIRE(b.recv_value == a.send_value);
		REQUIRE(a.recv_value == c.send_value);
	}

	SECTION("Multiple peers send to one (D→A, C→A, B→A)", "[RemoteEngineConnection]")
	{
		struct Sender
		{
			const char* id;
			int value = 0;
			RemoteEngineConnection sender;
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

		auto setup_receiver = [](RemoteEngineConnection& rx, const char* self, const char* sender_id, int* out)
		{
			rx.configure_receiver(self);
			rx.set_field_binder(
				[=](const char* path, RemoteEngineConnection::Field& f)
				{
					if (string_equals(path, sender_id))
					{
						f.path = path;
						f.recv_ptr = out;
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

		const int port_a_b = wait_for_listen_port(a.rx_b);
		const int port_a_c = wait_for_listen_port(a.rx_c);
		const int port_a_d = wait_for_listen_port(a.rx_d);
		REQUIRE(port_a_b > 0);
		REQUIRE(port_a_c > 0);
		REQUIRE(port_a_d > 0);

		auto setup_sender = [](RemoteEngineConnection& s, const char* from, const char* to, const char* path, int* value, int port)
		{
			s.configure_sender(from, to, "127.0.0.1", port);
			s.register_field({path, value, nullptr, sizeof(int), 0});
		};

		setup_sender(b.sender, "peer-b", "peer-a", "peer-b", &b.value, port_a_b);
		setup_sender(c.sender, "peer-c", "peer-a", "peer-c", &c.value, port_a_c);
		setup_sender(d.sender, "peer-d", "peer-a", "peer-d", &d.value, port_a_d);

		for (int i = 0; i < 200; ++i)
		{
			a.rx_b.tick(TICK_INFO_FIRST_10MS_100HZ);
			a.rx_c.tick(TICK_INFO_FIRST_10MS_100HZ);
			a.rx_d.tick(TICK_INFO_FIRST_10MS_100HZ);

			b.sender.tick(TICK_INFO_FIRST_10MS_100HZ);
			c.sender.tick(TICK_INFO_FIRST_10MS_100HZ);
			d.sender.tick(TICK_INFO_FIRST_10MS_100HZ);

			Thread::sleep_ms(10);

			if (a.recv_from_b == b.value && a.recv_from_c == c.value && a.recv_from_d == d.value)
				break;
		}

		REQUIRE(a.recv_from_b == b.value);
		REQUIRE(a.recv_from_c == c.value);
		REQUIRE(a.recv_from_d == d.value);
	}
}
