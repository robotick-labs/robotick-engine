#pragma once

#include <string>
#include <memory>
#include <functional>

#if defined(ROBOTICK_PLATFORM_ESP32)
// TODO: Include ESP32 MQTT client (e.g., esp-mqtt)
#else
#include <mqtt/async_client.h>
#endif

namespace robotick
{
    
	class MqttClient
	{
	public:
		MqttClient(const std::string& broker_uri, const std::string& client_id)
		{
#if !defined(ROBOTICK_PLATFORM_ESP32)
			client = std::make_unique<mqtt::async_client>(broker_uri, client_id);
			connect_options.set_clean_session(true);
#endif
		}

		void set_callback(std::function<void(const std::string&, const std::string&)> on_message)
		{
#if !defined(ROBOTICK_PLATFORM_ESP32)
			callback = std::make_shared<Callback>(on_message);
			client->set_callback(*callback);
#endif
		}

		void connect()
		{
#if !defined(ROBOTICK_PLATFORM_ESP32)
			client->connect(connect_options)->wait();
#endif
		}

		void subscribe(const std::string& topic, int qos = 1)
		{
#if !defined(ROBOTICK_PLATFORM_ESP32)
			client->subscribe(topic, qos)->wait();
#endif
		}

		void publish(const std::string& topic, const std::string& payload, bool retained = true)
		{
#if !defined(ROBOTICK_PLATFORM_ESP32)
			auto msg = mqtt::make_message(topic, payload);
			msg->set_retained(retained);
			client->publish(msg);
#endif
		}

	private:
#if !defined(ROBOTICK_PLATFORM_ESP32)
		class Callback : public virtual mqtt::callback
		{
		public:
			explicit Callback(std::function<void(const std::string&, const std::string&)> on_msg)
				: on_message(std::move(on_msg)) {}

			void message_arrived(mqtt::const_message_ptr msg) override
			{
				if (on_message) on_message(msg->get_topic(), msg->to_string());
			}

		private:
			std::function<void(const std::string&, const std::string&)> on_message;
		};

		std::unique_ptr<mqtt::async_client> client;
		mqtt::connect_options connect_options;
		std::shared_ptr<Callback> callback;
#else
		// TODO: Add ESP32 MQTT implementation here
#endif
	};

} // namespace robotick
