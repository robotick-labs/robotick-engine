#include "robotick/framework/data/RemoteEngineDiscoverer.h"
#include "robotick/api.h"
#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/common/StringUtils.h"
#include "robotick/platform/Atomic.h"
#include "robotick/platform/Thread.h"

#include <catch2/catch_all.hpp>

using namespace robotick;

TEST_CASE("Integration/Framework/Data/RemoteEngineDiscoverer")
{
	SECTION("Discovery exchange triggers callback with correct info")
	{
		const char* model_to_find = "target-model";
		const char* sender_model = "sender-model";

		AtomicValue<bool> requested(false);
		AtomicValue<bool> discovered(false);
		AtomicValue<bool> correct_info(false);

		RemoteEngineDiscoverer sender;
		RemoteEngineDiscoverer receiver;

		sender.initialize_sender(sender_model, model_to_find);
		receiver.initialize_receiver(model_to_find);

		receiver.set_on_incoming_connection_requested(
			[&](const char* source_id, uint16_t& port_out)
			{
				requested.store(true);
				REQUIRE(string_equals(source_id, sender_model));
				port_out = 7263;
			});
		sender.set_on_remote_model_discovered(
			[&](const RemoteEngineDiscoverer::PeerInfo& info)
			{
				discovered.store(true);
				if (info.model_id == model_to_find && info.port == 7263)
					correct_info.store(true);
			});

		for (int i = 0; i < 200 && !discovered.load(); ++i) // 2 second timeout for CI stability
		{
			sender.tick(TICK_INFO_FIRST_10MS_100HZ);
			receiver.tick(TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(10);
		}

		REQUIRE(requested.load());
		REQUIRE(discovered.load());
		REQUIRE(correct_info.load());
	}

	SECTION("No response if model name doesn't match")
	{
		AtomicValue<bool> called(false);

		RemoteEngineDiscoverer receiver;
		RemoteEngineDiscoverer sender;

		receiver.initialize_receiver("receiver-model");
		sender.initialize_sender("sender-model", "nonexistent-model");

		receiver.set_on_incoming_connection_requested(
			[](const char*, uint16_t& port_out)
			{
				port_out = 45681;
			});
		sender.set_on_remote_model_discovered(
			[&](const auto&)
			{
				called.store(true);
			});

		// Tick for timeout period - should NOT trigger callback
		for (int i = 0; i < 100; ++i)
		{
			receiver.tick(TICK_INFO_FIRST_10MS_100HZ);
			sender.tick(TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(10);
			if (called.load())
				break; // Early exit if unexpectedly called
		}

		REQUIRE_FALSE(called.load());
	}

	SECTION("Two models mutually discover with correct info")
	{
		const char* model_a = "alpha";
		const char* model_b = "beta";
		constexpr int port_a = 11111;
		constexpr int port_b = 22222;

		AtomicValue<bool> a_discovered(false), b_discovered(false);
		AtomicValue<bool> a_correct(false), b_correct(false);

		RemoteEngineDiscoverer recv_a, send_a;
		RemoteEngineDiscoverer recv_b, send_b;

		recv_a.initialize_receiver(model_a);
		send_a.initialize_sender(model_a, model_b);
		recv_b.initialize_receiver(model_b);
		send_b.initialize_sender(model_b, model_a);

		recv_a.set_on_incoming_connection_requested(
			[&](const char* src, uint16_t& out)
			{
				REQUIRE(string_equals(src, model_b));
				out = port_a;
			});
		recv_b.set_on_incoming_connection_requested(
			[&](const char* src, uint16_t& out)
			{
				REQUIRE(string_equals(src, model_a));
				out = port_b;
			});

		send_a.set_on_remote_model_discovered(
			[&](const auto& info)
			{
				a_discovered.store(true);
				if (info.model_id == model_b && info.port == port_b)
					a_correct.store(true);
			});
		send_b.set_on_remote_model_discovered(
			[&](const auto& info)
			{
				b_discovered.store(true);
				if (info.model_id == model_a && info.port == port_a)
					b_correct.store(true);
			});

		for (int i = 0; i < 200 && (!a_discovered.load() || !b_discovered.load()); ++i)
		{
			recv_a.tick(TICK_INFO_FIRST_10MS_100HZ);
			send_a.tick(TICK_INFO_FIRST_10MS_100HZ);
			recv_b.tick(TICK_INFO_FIRST_10MS_100HZ);
			send_b.tick(TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(5);
		}

		REQUIRE(a_discovered.load());
		REQUIRE(b_discovered.load());
		REQUIRE(a_correct.load());
		REQUIRE(b_correct.load());
	}

	SECTION("Reset allows rediscovery (with verification)")
	{
		const char* model_self = "gamma";
		const char* model_peer = "omega";
		constexpr uint16_t reply_port = 30303;

		AtomicValue<int> discovery_count(0);

		RemoteEngineDiscoverer recv;
		RemoteEngineDiscoverer send;

		recv.initialize_receiver(model_self);
		send.initialize_sender(model_peer, model_self);

		recv.set_on_incoming_connection_requested(
			[&](const char* src, uint16_t& out)
			{
				REQUIRE(string_equals(src, model_peer));
				out = reply_port;
			});
		send.set_on_remote_model_discovered(
			[&](const auto& info)
			{
				REQUIRE(info.model_id == model_self);
				REQUIRE(info.port == reply_port);
				discovery_count++;
			});

		for (int i = 0; i < 200 && discovery_count.load() < 1; ++i)
		{
			recv.tick(TICK_INFO_FIRST_10MS_100HZ);
			send.tick(TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(5);
		}

		send.reset_discovery();

		for (int i = 0; i < 200 && discovery_count.load() < 2; ++i)
		{
			recv.tick(TICK_INFO_FIRST_10MS_100HZ);
			send.tick(TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(5);
		}

		REQUIRE(discovery_count.load() >= 2);
	}
}
