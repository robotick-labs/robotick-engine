
// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/platform/WebServer.h"
#include "robotick/api.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <curl/curl.h>
#include <string>
#include <thread>

using namespace robotick;

namespace
{

	std::string http_post(const std::string& url, const std::string& body)
	{
		CURL* curl = curl_easy_init();
		std::string result;
		if (!curl)
			return result;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
		curl_easy_setopt(
			curl, CURLOPT_WRITEFUNCTION,
			+[](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t
			{
				auto& out = *reinterpret_cast<std::string*>(userdata);
				out.append(ptr, size * nmemb);
				return size * nmemb;
			});
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

		curl_easy_perform(curl);

		return result;
	}

	std::string http_get(const std::string& url)
	{
		CURL* curl = curl_easy_init();
		std::string result;
		if (!curl)
			return result;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2);
		curl_easy_setopt(
			curl, CURLOPT_WRITEFUNCTION,
			+[](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t
			{
				auto& out = *reinterpret_cast<std::string*>(userdata);
				out.append(ptr, size * nmemb);
				return size * nmemb;
			});
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

		curl_easy_perform(curl);

		return result;
	}

	void wait_for(std::atomic<bool>& flag, int timeout_ms = 500)
	{
		const int sleep_step_ms = 10;
		int waited = 0;
		while (!flag && waited < timeout_ms)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(sleep_step_ms));
			waited += sleep_step_ms;
		}
	}

} // namespace

TEST_CASE("Unit|Platform|WebServer|WebServer serves static file and fallback handler", "[WebServer]")
{
	std::atomic<bool> fallback_called{false};
	std::string last_method;
	std::string last_body;

	WebServer server;
	server.start(8089,
		[&](const WebRequest& request, WebResponse& response)
		{
			fallback_called = true;
			last_method = request.method;
			last_body = request.body;

			response.body = std::string("Custom: ") + std::string(request.uri);
			response.content_type = "text/plain";
		});

	std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Give server time to bind

	SECTION("Serves index.html")
	{
		std::string response = http_get("http://localhost:8089/index.html");

		ROBOTICK_INFO("Response was: '%s'", response.c_str());

		REQUIRE(response.find("Hello Test Page") != std::string::npos);
	}

	SECTION("Fallback handler is called for missing path with GET")
	{
		std::string response = http_get("http://localhost:8089/does_not_exist");
		wait_for(fallback_called);
		REQUIRE(fallback_called);
		REQUIRE(response.find("Custom: /does_not_exist") != std::string::npos);
		REQUIRE(last_method == "GET");
	}

	SECTION("Fallback handler is called for POST with body")
	{
		fallback_called = false;
		std::string response = http_post("http://localhost:8089/api/submit", "hello=world");
		wait_for(fallback_called);
		REQUIRE(fallback_called);
		REQUIRE(response.find("Custom: /api/submit") != std::string::npos);
		REQUIRE(last_method == "POST");
		REQUIRE(last_body == "hello=world");
	}

	server.stop();
}
