// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/platform/MqttClient.h"

namespace robotick
{
	MqttClient::MqttClient(const std::string& broker_uri, const std::string& client_id)
	{
#if !defined(ROBOTICK_PLATFORM_ESP32)
		client = std::make_unique<mqtt::async_client>(broker_uri, client_id);
		connect_options.set_clean_session(true);
#endif
	}

	void MqttClient::set_callback(std::function<void(const std::string&, const std::string&)> on_message)
	{
#if !defined(ROBOTICK_PLATFORM_ESP32)
		callback = std::make_shared<Callback>(std::move(on_message));
		client->set_callback(*callback);
#endif
	}

	void MqttClient::connect()
	{
#if !defined(ROBOTICK_PLATFORM_ESP32)
		client->connect(connect_options)->wait();
#endif
	}

	void MqttClient::subscribe(const std::string& topic, int qos)
	{
#if !defined(ROBOTICK_PLATFORM_ESP32)
		client->subscribe(topic, qos)->wait();
#endif
	}

	void MqttClient::publish(const std::string& topic, const std::string& payload, bool retained)
	{
#if !defined(ROBOTICK_PLATFORM_ESP32)
		auto msg = mqtt::make_message(topic, payload);
		msg->set_retained(retained);
		client->publish(msg);
#endif
	}

#if !defined(ROBOTICK_PLATFORM_ESP32)
	MqttClient::Callback::Callback(std::function<void(const std::string&, const std::string&)> on_msg) : on_message(std::move(on_msg))
	{
	}

	void MqttClient::Callback::message_arrived(mqtt::const_message_ptr msg)
	{
		if (on_message)
			on_message(msg->get_topic(), msg->to_string());
	}
#endif
} // namespace robotick
