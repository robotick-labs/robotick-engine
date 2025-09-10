#include "robotick/framework/data/RemoteEngineDiscoverer.h"
#include "robotick/api.h"
#include "robotick/platform/Threading.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <string>

using namespace robotick;

TEST_CASE("Integration/Framework/Data/RemoteEngineDiscoverer")
{
	SECTION("Discovery exchange triggers callback with correct info")
	{
		const char* model_to_find = "target-model";
		const char* sender_model = "sender-model";

		std::atomic<bool> requested{false};
		std::atomic<bool> discovered{false};
		std::atomic<bool> correct_info{false};

		RemoteEngineDiscoverer sender;
		RemoteEngineDiscoverer receiver;

		sender.initialize_sender(sender_model, model_to_find);
		receiver.initialize_receiver(model_to_find);

		receiver.set_on_incoming_connection_requested(
			[&](const char* source_id, int& port_out)
			{
				requested = true;
				REQUIRE(std::string(source_id) == sender_model);
				port_out = 7263;
			});
		sender.set_on_remote_model_discovered(
			[&](const RemoteEngineDiscoverer::PeerInfo& info)
			{
				discovered = true;
				if (info.model_id == model_to_find && info.port == 7263)
					correct_info = true;
			});

		for (int i = 0; i < 100 && !discovered; ++i)
		{
			sender.tick(TICK_INFO_FIRST_10MS_100HZ);
			receiver.tick(TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(10);
		}

		REQUIRE(requested);
		REQUIRE(discovered);
		REQUIRE(correct_info);
	}

	SECTION("No response if model name doesn't match")
	{
		std::atomic<bool> called{false};

		RemoteEngineDiscoverer receiver;
		RemoteEngineDiscoverer sender;

		receiver.initialize_receiver("receiver-model");
		sender.initialize_sender("sender-model", "nonexistent-model");

		receiver.set_on_incoming_connection_requested(
			[](const char*, int& port_out)
			{
				port_out = 45681;
			});
		sender.set_on_remote_model_discovered(
			[&](const auto&)
			{
				called = true;
			});

		for (int i = 0; i < 100 && !called; ++i)
		{
			receiver.tick(TICK_INFO_FIRST_10MS_100HZ);
			sender.tick(TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(10);
		}

		REQUIRE_FALSE(called);
	}

	SECTION("Two models mutually discover with correct info")
	{
		const char* model_a = "alpha";
		const char* model_b = "beta";
		constexpr int port_a = 11111;
		constexpr int port_b = 22222;

		std::atomic<bool> a_discovered{false}, b_discovered{false};
		std::atomic<bool> a_correct{false}, b_correct{false};

		RemoteEngineDiscoverer recv_a, send_a;
		RemoteEngineDiscoverer recv_b, send_b;

		recv_a.initialize_receiver(model_a);
		send_a.initialize_sender(model_a, model_b);
		recv_b.initialize_receiver(model_b);
		send_b.initialize_sender(model_b, model_a);

		recv_a.set_on_incoming_connection_requested(
			[&](const char* src, int& out)
			{
				REQUIRE(std::string(src) == model_b);
				out = port_a;
			});
		recv_b.set_on_incoming_connection_requested(
			[&](const char* src, int& out)
			{
				REQUIRE(std::string(src) == model_a);
				out = port_b;
			});

		send_a.set_on_remote_model_discovered(
			[&](const auto& info)
			{
				a_discovered = true;
				if (info.model_id == model_b && info.port == port_b)
					a_correct = true;
			});
		send_b.set_on_remote_model_discovered(
			[&](const auto& info)
			{
				b_discovered = true;
				if (info.model_id == model_a && info.port == port_a)
					b_correct = true;
			});

		for (int i = 0; i < 200 && (!a_discovered || !b_discovered); ++i)
		{
			recv_a.tick(TICK_INFO_FIRST_10MS_100HZ);
			send_a.tick(TICK_INFO_FIRST_10MS_100HZ);
			recv_b.tick(TICK_INFO_FIRST_10MS_100HZ);
			send_b.tick(TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(5);
		}

		REQUIRE(a_discovered);
		REQUIRE(b_discovered);
		REQUIRE(a_correct);
		REQUIRE(b_correct);
	}

	SECTION("Reset allows rediscovery (with verification)")
	{
		const char* model_self = "gamma";
		const char* model_peer = "omega";
		constexpr int reply_port = 30303;

		std::atomic<int> discovery_count{0};

		RemoteEngineDiscoverer recv;
		RemoteEngineDiscoverer send;

		recv.initialize_receiver(model_self);
		send.initialize_sender(model_peer, model_self);

		recv.set_on_incoming_connection_requested(
			[&](const char* src, int& out)
			{
				REQUIRE(std::string(src) == model_peer);
				out = reply_port;
			});
		send.set_on_remote_model_discovered(
			[&](const auto& info)
			{
				REQUIRE(info.model_id == model_self);
				REQUIRE(info.port == reply_port);
				discovery_count++;
			});

		for (int i = 0; i < 200 && discovery_count < 1; ++i)
		{
			recv.tick(TICK_INFO_FIRST_10MS_100HZ);
			send.tick(TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(5);
		}

		send.reset_discovery();

		for (int i = 0; i < 200 && discovery_count < 2; ++i)
		{
			recv.tick(TICK_INFO_FIRST_10MS_100HZ);
			send.tick(TICK_INFO_FIRST_10MS_100HZ);
			Thread::sleep_ms(5);
		}

		REQUIRE(discovery_count >= 2);
	}
}
