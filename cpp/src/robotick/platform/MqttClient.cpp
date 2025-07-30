// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#if defined(ROBOTICK_PLATFORM_DESKTOP)

#include "robotick/platform/MqttClient.h"

namespace robotick
{
	MqttClient::MqttClient(const std::string& broker_uri, const std::string& client_id)
	{
		client = std::make_unique<mqtt::async_client>(broker_uri, client_id);
		connect_options.set_clean_session(true);
	}

	void MqttClient::set_callback(std::function<void(const std::string&, const std::string&)> on_message)
	{
		callback = std::make_shared<Callback>(std::move(on_message));
		client->set_callback(*callback);
	}

	void MqttClient::connect()
	{
		client->connect(connect_options)->wait();
	}

	void MqttClient::subscribe(const std::string& topic, int qos)
	{
		client->subscribe(topic, qos)->wait();
	}

	void MqttClient::publish(const std::string& topic, const std::string& payload, bool retained)
	{
		auto msg = mqtt::make_message(topic, payload);
		msg->set_retained(retained);
		client->publish(msg);
	}

	MqttClient::Callback::Callback(std::function<void(const std::string&, const std::string&)> on_msg) : on_message(std::move(on_msg))
	{
	}

	void MqttClient::Callback::message_arrived(mqtt::const_message_ptr msg)
	{
		if (on_message)
			on_message(msg->get_topic(), msg->to_string());
	}
} // namespace robotick

#endif // defined(ROBOTICK_PLATFORM_DESKTOP)
