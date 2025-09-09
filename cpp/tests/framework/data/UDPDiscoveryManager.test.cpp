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
}
