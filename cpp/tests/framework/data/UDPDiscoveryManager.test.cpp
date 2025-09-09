// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/UDPDiscoveryManager.h"
#include "robotick/api.h"
#include "robotick/platform/Threading.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <string>
#include <vector>

using namespace robotick;

TEST_CASE("Integration/Framework/Data/UDPDiscoveryManager")
{
	SECTION("Discovery exchange triggers callback", "[UDPDiscoveryManager]")
	{
		constexpr int sender_port = 7262;
		constexpr int receiver_port = 7263;
		const char* model_to_find = "target-model";

		std::atomic<bool> callback_triggered{false};
		UDPDiscoveryManager receiver;
		UDPDiscoveryManager sender;

		receiver.initialize(model_to_find, receiver_port, DiscoveryMode::Receiver);
		sender.initialize("unrelated-model", sender_port, DiscoveryMode::Sender);

		receiver.set_on_discovered(
			[&](const UDPDiscoveryManager::PeerInfo&)
			{
				FAIL("Receiver should not receive discovery replies");
			});

		sender.set_on_discovered(
			[&](const UDPDiscoveryManager::PeerInfo& info)
			{
				ROBOTICK_INFO("Callback triggered with model_id=%s ip=%s port=%d", info.model_id.c_str(), info.ip.c_str(), info.port);
				callback_triggered = true;
			});

		sender.broadcast_discovery_request(model_to_find);

		for (int i = 0; i < 50; ++i)
		{
			receiver.tick();
			sender.tick();
			Thread::sleep_ms(10);
		}

		REQUIRE(callback_triggered);
	}

	SECTION("Does not respond if model name does not match", "[UDPDiscoveryManager]")
	{
		constexpr int sender_port = 45680;
		constexpr int receiver_port = 45681;

		std::atomic<bool> callback_triggered{false};
		UDPDiscoveryManager receiver;
		UDPDiscoveryManager sender;

		receiver.initialize("receiver-model", receiver_port, DiscoveryMode::Receiver);
		sender.initialize("unrelated-model", sender_port, DiscoveryMode::Sender);

		sender.set_on_discovered(
			[&](const UDPDiscoveryManager::PeerInfo&)
			{
				callback_triggered = true;
			});

		sender.broadcast_discovery_request("nonexistent-model");

		for (int i = 0; i < 50; ++i)
		{
			receiver.tick();
			sender.tick();
			Thread::sleep_ms(10);
		}

		REQUIRE_FALSE(callback_triggered);
	}

	SECTION("Multiple requests still work", "[UDPDiscoveryManager]")
	{
		constexpr int sender_port = 45682;
		constexpr int receiver_port = 45683;
		const char* model_to_find = "common-model";

		std::atomic<int> hit_count{0};
		UDPDiscoveryManager receiver;
		UDPDiscoveryManager sender;

		receiver.initialize(model_to_find, receiver_port, DiscoveryMode::Receiver);
		sender.initialize("other-model", sender_port, DiscoveryMode::Sender);

		sender.set_on_discovered(
			[&](const UDPDiscoveryManager::PeerInfo&)
			{
				hit_count++;
			});

		for (int i = 0; i < 3; ++i)
		{
			sender.broadcast_discovery_request(model_to_find);
			for (int j = 0; j < 20; ++j)
			{
				receiver.tick();
				sender.tick();
				Thread::sleep_ms(5);
			}
		}

		REQUIRE(hit_count >= 1);
	}

	SECTION("Two models mutually discover each other (Sender + Receiver per model)", "[UDPDiscoveryManager]")
	{
		constexpr int recv_port1 = 45720;
		constexpr int recv_port2 = 45721;
		constexpr int send_port1 = 45722;
		constexpr int send_port2 = 45723;

		const char* model1 = "alpha-model";
		const char* model2 = "beta-model";

		std::atomic<int> discovered1{0};
		std::atomic<int> discovered2{0};

		// Model 1: Receiver + Sender
		UDPDiscoveryManager receiver1;
		UDPDiscoveryManager sender1;

		receiver1.initialize(model1, recv_port1, DiscoveryMode::Receiver);
		sender1.initialize(model1, send_port1, DiscoveryMode::Sender);

		// Model 2: Receiver + Sender
		UDPDiscoveryManager receiver2;
		UDPDiscoveryManager sender2;

		receiver2.initialize(model2, recv_port2, DiscoveryMode::Receiver);
		sender2.initialize(model2, send_port2, DiscoveryMode::Sender);

		sender1.set_on_discovered(
			[&](const UDPDiscoveryManager::PeerInfo& info)
			{
				ROBOTICK_INFO("Model1 sender discovered %s", info.model_id.c_str());
				discovered1++;
			});
		sender2.set_on_discovered(
			[&](const UDPDiscoveryManager::PeerInfo& info)
			{
				ROBOTICK_INFO("Model2 sender discovered %s", info.model_id.c_str());
				discovered2++;
			});

		// Each sender sends a discovery request for the other model
		sender1.broadcast_discovery_request(model2);
		sender2.broadcast_discovery_request(model1);

		for (int i = 0; i < 100; ++i)
		{
			receiver1.tick();
			sender1.tick();
			receiver2.tick();
			sender2.tick();
			Thread::sleep_ms(10);
		}

		REQUIRE(discovered1 >= 1);
		REQUIRE(discovered2 >= 1);
	}

	SECTION("Three models mutually discover each other (Sender + Receiver per model)", "[UDPDiscoveryManager]")
	{
		// Receiver ports
		constexpr int recv_port1 = 45730;
		constexpr int recv_port2 = 45731;
		constexpr int recv_port3 = 45732;

		// Sender ports
		constexpr int send_port1 = 45733;
		constexpr int send_port2 = 45734;
		constexpr int send_port3 = 45735;

		const char* model1 = "model-one";
		const char* model2 = "model-two";
		const char* model3 = "model-three";

		std::atomic<int> discovered1{0};
		std::atomic<int> discovered2{0};
		std::atomic<int> discovered3{0};

		// Model 1: Receiver + Sender
		UDPDiscoveryManager receiver1;
		UDPDiscoveryManager sender1;
		receiver1.initialize(model1, recv_port1, DiscoveryMode::Receiver);
		sender1.initialize(model1, send_port1, DiscoveryMode::Sender);
		sender1.set_on_discovered(
			[&](const UDPDiscoveryManager::PeerInfo& info)
			{
				ROBOTICK_INFO("Model1 sender discovered %s", info.model_id.c_str());
				discovered1++;
			});

		// Model 2: Receiver + Sender
		UDPDiscoveryManager receiver2;
		UDPDiscoveryManager sender2;
		receiver2.initialize(model2, recv_port2, DiscoveryMode::Receiver);
		sender2.initialize(model2, send_port2, DiscoveryMode::Sender);
		sender2.set_on_discovered(
			[&](const UDPDiscoveryManager::PeerInfo& info)
			{
				ROBOTICK_INFO("Model2 sender discovered %s", info.model_id.c_str());
				discovered2++;
			});

		// Model 3: Receiver + Sender
		UDPDiscoveryManager receiver3;
		UDPDiscoveryManager sender3;
		receiver3.initialize(model3, recv_port3, DiscoveryMode::Receiver);
		sender3.initialize(model3, send_port3, DiscoveryMode::Sender);
		sender3.set_on_discovered(
			[&](const UDPDiscoveryManager::PeerInfo& info)
			{
				ROBOTICK_INFO("Model3 sender discovered %s", info.model_id.c_str());
				discovered3++;
			});

		// Each sender tries to discover the other two
		sender1.broadcast_discovery_request(model2);
		sender1.broadcast_discovery_request(model3);

		sender2.broadcast_discovery_request(model1);
		sender2.broadcast_discovery_request(model3);

		sender3.broadcast_discovery_request(model1);
		sender3.broadcast_discovery_request(model2);

		// Run ticks long enough for exchange to complete
		for (int i = 0; i < 100; ++i)
		{
			receiver1.tick();
			sender1.tick();
			receiver2.tick();
			sender2.tick();
			receiver3.tick();
			sender3.tick();
			Thread::sleep_ms(10);
		}

		REQUIRE(discovered1 >= 2);
		REQUIRE(discovered2 >= 2);
		REQUIRE(discovered3 >= 2);
	}
}
