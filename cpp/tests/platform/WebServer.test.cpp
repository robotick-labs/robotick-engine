// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include "robotick/framework/concurrency/Thread.h"
#include "robotick/framework/services/WebServer.h"

#include <catch2/catch_all.hpp>
#include <curl/curl.h>

#include <cstdio>
#include <cstring>

using namespace robotick;

// ----------------------------------------------------------
// Curl helpers
// ----------------------------------------------------------
namespace
{
struct TextBuffer
	{
		char data[4096] = {};
		size_t len = 0;

		void clear()
		{
			len = 0;
			data[0] = '\0';
		}

		void append(const char* src, size_t count)
		{
			if (!src || count == 0)
				return;
			const size_t cap = sizeof(data) - 1;
			const size_t space = (len < cap) ? (cap - len) : 0;
			const size_t to_copy = (count < space) ? count : space;
			if (to_copy > 0)
			{
				::memcpy(data + len, src, to_copy);
				len += to_copy;
				data[len] = '\0';
			}
		}
	};

	TextBuffer http_post(const char* url, const char* body)
	{
		CURL* curl = curl_easy_init();
		TextBuffer result;
		if (!curl)
			return result;

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body ? ::strlen(body) : 0);
		curl_easy_setopt(
			curl,
			CURLOPT_WRITEFUNCTION,
			+[](char* ptr, size_t sz, size_t nm, void* ud) -> size_t
			{
				auto& out = *reinterpret_cast<TextBuffer*>(ud);
				const size_t bytes = sz * nm;
				out.append(ptr, bytes);
				return bytes;
			});
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

		curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		return result;
	}

	TextBuffer http_get(const char* url)
	{
		CURL* curl = curl_easy_init();
		TextBuffer result;
		if (!curl)
			return result;

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
		curl_easy_setopt(
			curl,
			CURLOPT_WRITEFUNCTION,
			+[](char* ptr, size_t sz, size_t nm, void* ud) -> size_t
			{
				auto& out = *reinterpret_cast<TextBuffer*>(ud);
				const size_t bytes = sz * nm;
				out.append(ptr, bytes);
				return bytes;
			});
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

		curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		return result;
	}

	void wait_for(volatile bool& flag, int timeout_ms = 500)
	{
		constexpr int step = 10;
		int waited = 0;
		while (!flag && waited < timeout_ms)
		{
			Thread::sleep_ms(step);
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

		ScopedTestServer(const char* name, const char* docroot, WebRequestHandler handler)
		{
			server.start(name, 0, docroot, handler);
			Thread::sleep_ms(100);
			port = server.get_bound_port();
			REQUIRE(port != 0);
		}

		~ScopedTestServer() { server.stop(); }

		void url(char* out, size_t out_size, const char* path) const
		{
			::snprintf(out, out_size, "http://localhost:%u%s", (unsigned)port, path);
		}
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
			"data/remote_control_interface_web",
			[](const WebRequest&, WebResponse&)
			{
				return false;
			});

		char url[128];
		S.url(url, sizeof(url), "/index.html");
		TextBuffer body = http_get(url);
		REQUIRE(::strstr(body.data, "Hello Test Page") != nullptr);
	}

	// -----------------------------------------------------------------------------------
	// 2. Function-based GET
	// -----------------------------------------------------------------------------------
	SECTION("Function-based handler GET")
	{
		volatile bool called = false;
		TextBuffer method;
		TextBuffer last_uri;

		ScopedTestServer S("Function-based GET",
			nullptr,
			[&](const WebRequest& req, WebResponse& resp)
			{
				called = true;
				method.clear();
				last_uri.clear();
				method.append(req.method.c_str(), req.method.length());
				last_uri.append(req.uri.c_str(), req.uri.length());

				resp.set_content_type("text/plain");
				TextBuffer body;
				body.append("Custom: ", 8);
				body.append(last_uri.data, last_uri.len);
				resp.set_body_string(body.data);
				return true;
			});

		char url[128];
		S.url(url, sizeof(url), "/does_not_exist");
		TextBuffer body = http_get(url);

		wait_for(called);

		REQUIRE(called);
		REQUIRE(::strcmp(method.data, "GET") == 0);
		REQUIRE(::strcmp(last_uri.data, "/does_not_exist") == 0);
		REQUIRE(::strstr(body.data, "Custom: /does_not_exist") != nullptr);
	}

	// -----------------------------------------------------------------------------------
	// 3. Function-based POST with body
	// -----------------------------------------------------------------------------------
	SECTION("Function-based handler POST with body")
	{
		volatile bool called = false;
		TextBuffer method;
		TextBuffer posted;

		ScopedTestServer S("Function-based POST",
			nullptr,
			[&](const WebRequest& req, WebResponse& resp)
			{
				called = true;
				method.clear();
				method.append(req.method.c_str(), req.method.length());
				if (req.body.size() > 0)
				{
					posted.clear();
					posted.append(reinterpret_cast<const char*>(req.body.begin()), req.body.size());
				}

				resp.set_content_type("text/plain");
				TextBuffer body;
				body.append("POST: ", 6);
				body.append(req.uri.c_str(), req.uri.length());
				resp.set_body_string(body.data);
				return true;
			});

		char url[128];
		S.url(url, sizeof(url), "/api/x");
		TextBuffer body = http_post(url, "hello=world");
		wait_for(called);

		REQUIRE(called);
		REQUIRE(::strcmp(method.data, "POST") == 0);
		REQUIRE(::strcmp(posted.data, "hello=world") == 0);
		REQUIRE(::strstr(body.data, "POST: /api/x") != nullptr);
	}

	// ===================================================================================
	// 4. Pure streaming contract: ordering violations must ROBOTICK_ERROR
	// ===================================================================================

	SECTION("Handler returns true but sets no body -> empty 200 OK")
	{
		ScopedTestServer S("NoBody",
			nullptr,
			[](const WebRequest&, WebResponse&)
			{
				// Return true but do not send body.
				return true;
			});

		char url[128];
		S.url(url, sizeof(url), "/x");
		TextBuffer body = http_get(url);

		// Should NOT be fatal.
		// Should return valid HTTP 200 with empty body.
		REQUIRE(body.len == 0);
	}
}
