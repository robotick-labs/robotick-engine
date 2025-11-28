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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace robotick;

// ----------------------------------------------------------
// Curl helpers
// ----------------------------------------------------------
namespace
{
	std::atomic<uint16_t> g_webserver_port_counter{50000};

	uint16_t find_free_port()
	{
		int sock = ::socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0)
			return 0;

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		addr.sin_port = 0;

		if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
		{
			::close(sock);
			return 0;
		}

		socklen_t addr_len = sizeof(addr);
		if (::getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &addr_len) != 0)
		{
			::close(sock);
			return 0;
		}

		uint16_t port = ntohs(addr.sin_port);
		::close(sock);
		return port;
	}

	uint16_t allocate_test_port()
	{
		uint16_t port = find_free_port();
		if (port != 0)
			return port;
		return g_webserver_port_counter.fetch_add(1);
	}

	std::string http_post(const std::string& url, const std::string& body)
	{
		CURL* curl = curl_easy_init();
		std::string result;
		if (!curl)
			return result;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
		curl_easy_setopt(
			curl,
			CURLOPT_WRITEFUNCTION,
			+[](char* ptr, size_t sz, size_t nm, void* ud) -> size_t
			{
				auto& out = *reinterpret_cast<std::string*>(ud);
				out.append(ptr, sz * nm);
				return sz * nm;
			});
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

		curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		return result;
	}

	std::string http_get(const std::string& url)
	{
		CURL* curl = curl_easy_init();
		std::string result;
		if (!curl)
			return result;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
		curl_easy_setopt(
			curl,
			CURLOPT_WRITEFUNCTION,
			+[](char* ptr, size_t sz, size_t nm, void* ud) -> size_t
			{
				auto& out = *reinterpret_cast<std::string*>(ud);
				out.append(ptr, sz * nm);
				return sz * nm;
			});
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

		curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		return result;
	}

	void wait_for(std::atomic<bool>& flag, int timeout_ms = 500)
	{
		constexpr int step = 10;
		int waited = 0;
		while (!flag && waited < timeout_ms)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(step));
			waited += step;
		}
	}

	// ------------------------------------------------------
	// Simple RAII test server wrapper
	// ------------------------------------------------------
	struct ScopedTestServer
	{
		WebServer server;
		uint16_t port = 0;

		ScopedTestServer(const char* name, uint16_t port_in, const char* docroot, WebRequestHandler handler)
			: port(port_in)
		{
			server.start(name, port, docroot, handler);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		~ScopedTestServer() { server.stop(); }

		std::string url(const char* path) const { return "http://localhost:" + std::to_string(port) + path; }
	};

} // namespace

// =======================================================================================
// FINAL TEST SUITE
// =======================================================================================
TEST_CASE("Unit/Platform/WebServer")
{
	// -----------------------------------------------------------------------------------
	// 1. Serving static file
	// -----------------------------------------------------------------------------------
	SECTION("Serves index.html from document root")
	{
		ScopedTestServer S("StaticTest",
			allocate_test_port(),
			"data/remote_control_interface_web",
			[](const WebRequest&, WebResponse&)
			{
				return false;
			});

		std::string body = http_get(S.url("/index.html").c_str());
		REQUIRE(body.find("Hello Test Page") != std::string::npos);
	}

	// -----------------------------------------------------------------------------------
	// 2. Function-based GET
	// -----------------------------------------------------------------------------------
	SECTION("Function-based handler GET")
	{
		std::atomic<bool> called{false};
		std::string method;
		std::string last_uri;

		ScopedTestServer S("Function-based GET",
			allocate_test_port(),
			nullptr,
			[&](const WebRequest& req, WebResponse& resp)
			{
				called.store(true);
				method = req.method.c_str();
				last_uri = req.uri.c_str();

				resp.set_content_type("text/plain");
				std::string body = "Custom: " + last_uri;
				resp.set_body_string(body.c_str());
				return true;
			});

		std::string body = http_get(S.url("/does_not_exist").c_str());

		wait_for(called);

		REQUIRE(called.load());
		REQUIRE(method == "GET");
		REQUIRE(body.find("Custom: /does_not_exist") != std::string::npos);
	}

	// -----------------------------------------------------------------------------------
	// 3. Function-based POST with body
	// -----------------------------------------------------------------------------------
	SECTION("Function-based handler POST with body")
	{
		std::atomic<bool> called{false};
		std::string method;
		std::string posted;

		ScopedTestServer S("Function-based POST",
			allocate_test_port(),
			nullptr,
			[&](const WebRequest& req, WebResponse& resp)
			{
				called.store(true);
				method = req.method.c_str();
				if (req.body.size() > 0)
					posted.assign((const char*)req.body.begin(), req.body.size());

				resp.set_content_type("text/plain");
				std::string body = std::string("POST: ") + req.uri.c_str();
				resp.set_body_string(body.c_str());
				return true;
			});

		std::string body = http_post(S.url("/api/x").c_str(), "hello=world");
		wait_for(called);

		REQUIRE(called.load());
		REQUIRE(method == "POST");
		REQUIRE(posted == "hello=world");
		REQUIRE(body.find("POST: /api/x") != std::string::npos);
	}

	// ===================================================================================
	// 4. Pure streaming contract: ordering violations must ROBOTICK_ERROR
	// ===================================================================================

	SECTION("Handler returns true but sets no body -> empty 200 OK")
	{
		ScopedTestServer S("NoBody",
			allocate_test_port(),
			nullptr,
			[](const WebRequest&, WebResponse&)
			{
				// Return true but do not send body.
				return true;
			});

		std::string body = http_get(S.url("/x").c_str());

		// Should NOT be fatal.
		// Should return valid HTTP 200 with empty body.
		REQUIRE(body.size() == 0);
	}
}
