// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api.h"
#include "robotick/framework/data/Blackboard.h"

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

#if defined(ROBOTICK_PLATFORM_ESP32)
// TODO: add ESP32-specific includes later
#else
#include <mqtt/async_client.h>
#endif

namespace robotick
{

	struct MqttClientConfig
	{
		FixedString64 broker_url = "tcp://localhost";
		int port = 1883;
		FixedString64 topic_namespace = "robotick";
	};

	ROBOTICK_DEFINE_WORKLOAD(MqttClientWorkload)
	{
		MqttClientConfig config;

		struct State
		{
#if defined(ROBOTICK_PLATFORM_ESP32)
			// TODO: ESP32 mqtt client stub
#else
			std::unique_ptr<mqtt::async_client> client;
			mqtt::connect_options conn_opts;
#endif
			std::unordered_map<std::string, nlohmann::json> last_published;
			std::unordered_set<std::string> dirty_topics;
		} state;

		void init(const MqttClientConfig& cfg)
		{
			config = cfg;

#if !defined(ROBOTICK_PLATFORM_ESP32)
			std::string server_uri = config.broker_url.str() + ":" + std::to_string(config.port);
			state.client = std::make_unique<mqtt::async_client>(server_uri, "robotick_mqtt_client");

			state.conn_opts.set_clean_session(true);

			state.client->set_callback(
				[this](mqtt::const_message_ptr msg)
				{
					std::string topic = msg->get_topic();
					state.dirty_topics.insert(topic);
				});

			state.client->connect(state.conn_opts)->wait();
#endif
		}

		void tick(double delta_time, Blackboard& config, Blackboard& inputs, Blackboard& outputs)
		{
			// STUB: we'll use real workload iteration once available
			for (const auto& topic : state.dirty_topics)
			{
				// TODO: parse and apply updates
			}
			state.dirty_topics.clear();

			// STUB: simulate one output update
			std::string topic = config.topic_namespace.str() + "/example/outputs/position";
			nlohmann::json payload = 123; // e.g. outputs.get<int>("position")

			if (state.last_published[topic] != payload)
			{
#if !defined(ROBOTICK_PLATFORM_ESP32)
				auto msg = mqtt::make_message(topic, payload.dump());
				msg->set_retained(true);
				state.client->publish(msg);
#endif
				state.last_published[topic] = payload;
			}
		}
	};

	// TODO: replace this with a real discovery mechanism
	inline std::vector<MqttClientWorkload*> get_all_workload_instances()
	{
		return {};
	}

} // namespace robotick
