#pragma once

#include <functional>
#include <memory>
#include <string>

#if defined(ROBOTICK_PLATFORM_ESP32)
// TODO: Include ESP32 MQTT client (e.g., esp-mqtt)
#else
#include <mqtt/async_client.h>
#endif

namespace robotick
{

	class IMqttClient
	{
	  public:
		virtual ~IMqttClient() = default;

		virtual void connect() = 0;
		virtual void subscribe(const std::string& topic, int qos = 1) = 0;
		virtual void publish(const std::string& topic, const std::string& payload, bool retained = true) = 0;

		virtual void set_callback(std::function<void(const std::string&, const std::string&)> on_message) = 0;
	};

	class MqttClient : public IMqttClient
	{
	  public:
		MqttClient(const std::string& broker_uri, const std::string& client_id);

		void set_callback(std::function<void(const std::string&, const std::string&)>) override;
		void connect() override;
		void subscribe(const std::string& topic, int qos = 1) override;
		void publish(const std::string& topic, const std::string& payload, bool retained = true) override;

	  private:
#if !defined(ROBOTICK_PLATFORM_ESP32)
		class Callback : public virtual mqtt::callback
		{
		  public:
			explicit Callback(std::function<void(const std::string&, const std::string&)> on_msg);
			void message_arrived(mqtt::const_message_ptr msg) override;

		  private:
			std::function<void(const std::string&, const std::string&)> on_message;
		};

		std::unique_ptr<mqtt::async_client> client;
		mqtt::connect_options connect_options;
		std::shared_ptr<Callback> callback;
#endif
	};

} // namespace robotick
