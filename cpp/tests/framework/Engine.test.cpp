// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/config/AssertUtils.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/concurrency/Atomic.h"
#include "robotick/framework/concurrency/Thread.h"
#include "robotick/framework/data/DataConnection.h"
#include "robotick/framework/data/TelemetryServer.h"
#include "robotick/framework/json/Json.h"

#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <curl/curl.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace robotick::test
{
	namespace
	{
		struct HttpTextBuffer
		{
			char data[16384] = {};
			size_t len = 0;

			void append(const char* src, size_t count)
			{
				if (!src || count == 0)
					return;
				const size_t cap = sizeof(data) - 1;
				const size_t room = len < cap ? cap - len : 0;
				const size_t to_copy = room < count ? room : count;
				if (to_copy > 0)
				{
					::memcpy(data + len, src, to_copy);
					len += to_copy;
					data[len] = '\0';
				}
			}
		};

		struct HttpResponse
		{
			long status_code = 0;
			HttpTextBuffer body;
			HttpTextBuffer session_id;
		};

		size_t curl_write_to_text(char* ptr, size_t size, size_t nmemb, void* userdata)
		{
			auto& out = *static_cast<HttpTextBuffer*>(userdata);
			const size_t bytes = size * nmemb;
			out.append(ptr, bytes);
			return bytes;
		}

		size_t curl_capture_headers(char* ptr, size_t size, size_t nmemb, void* userdata)
		{
			auto& response = *static_cast<HttpResponse*>(userdata);
			const size_t bytes = size * nmemb;
			const char* prefix = "X-Robotick-Session-Id:";
			if (bytes > ::strlen(prefix) && ::strncasecmp(ptr, prefix, ::strlen(prefix)) == 0)
			{
				const char* value = ptr + ::strlen(prefix);
				while (*value == ' ' || *value == '\t')
					++value;
				const char* end = value + bytes;
				while (end > value && (end[-1] == '\r' || end[-1] == '\n'))
					--end;
				response.session_id.append(value, static_cast<size_t>(end - value));
			}
			return bytes;
		}

		bool contains_text(const char* haystack, const char* needle)
		{
			return haystack && needle && ::strstr(haystack, needle) != nullptr;
		}

		HttpResponse http_request(const char* url, const char* method = "GET", const char* body = nullptr)
		{
			CURL* curl = curl_easy_init();
			REQUIRE(curl != nullptr);

			HttpResponse response;
			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_to_text);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_capture_headers);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);
			if (::strcmp(method, "POST") == 0)
			{
				curl_easy_setopt(curl, CURLOPT_POST, 1L);
				curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body ? body : "");
				curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body ? static_cast<long>(::strlen(body)) : 0L);
				struct curl_slist* headers = nullptr;
				headers = curl_slist_append(headers, "Content-Type: application/json");
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
				curl_easy_perform(curl);
				curl_slist_free_all(headers);
			}
			else
			{
				curl_easy_perform(curl);
			}

			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
			curl_easy_cleanup(curl);
			return response;
		}

		struct EngineRunContext
		{
			Engine* engine = nullptr;
			AtomicFlag* stop_flag = nullptr;
		};

		class EngineRunThread
		{
		  public:
			EngineRunThread(Engine& engine, AtomicFlag& stop_flag)
			{
				context.engine = &engine;
				context.stop_flag = &stop_flag;
				thread = Thread(&EngineRunThread::run_entry, &context, "EngineRunThread");
			}

			~EngineRunThread()
			{
				if (thread.is_joining_supported() && thread.is_joinable())
				{
					thread.join();
				}
			}

			EngineRunThread(const EngineRunThread&) = delete;
			EngineRunThread& operator=(const EngineRunThread&) = delete;
			EngineRunThread(EngineRunThread&&) = delete;
			EngineRunThread& operator=(EngineRunThread&&) = delete;

		  private:
			static void run_entry(void* user_data)
			{
				auto* ctx = static_cast<EngineRunContext*>(user_data);
				if (ctx->engine && ctx->stop_flag)
				{
					ctx->engine->run(*ctx->stop_flag);
				}
			}

			EngineRunContext context{};
			Thread thread;
		};
	} // namespace
	struct TestSequencedGroupWorkload
	{
	};

	ROBOTICK_REGISTER_WORKLOAD(TestSequencedGroupWorkload)

	struct OverrunWorkload
	{
		void tick(const TickInfo& tick_info)
		{
			(void)tick_info;
			Thread::sleep_ms(10);
		}
	};

	ROBOTICK_REGISTER_WORKLOAD(OverrunWorkload)

	namespace
	{
		// === DummyWorkload (with config/inputs/load) ===

		struct DummyConfig
		{
			int value = 0;
		};
		ROBOTICK_REGISTER_STRUCT_BEGIN(DummyConfig)
		ROBOTICK_STRUCT_FIELD(DummyConfig, int, value)
		ROBOTICK_REGISTER_STRUCT_END(DummyConfig)

		struct DummyInputs
		{
			float input_float = 0.f;
			FixedString64 input_string_64;
		};
		ROBOTICK_REGISTER_STRUCT_BEGIN(DummyInputs)
		ROBOTICK_STRUCT_FIELD(DummyInputs, float, input_float)
		ROBOTICK_STRUCT_FIELD(DummyInputs, FixedString64, input_string_64)
		ROBOTICK_REGISTER_STRUCT_END(DummyInputs)

		struct DummyWorkload
		{
			DummyConfig config;
			DummyInputs inputs;
		};
		ROBOTICK_REGISTER_WORKLOAD(DummyWorkload, DummyConfig, DummyInputs)

		// === TickCounterWorkload ===

		struct TickCounterWorkload
		{
			int count = 0;
			void tick(const TickInfo&) { count++; }
		};
		ROBOTICK_REGISTER_WORKLOAD(TickCounterWorkload)

		struct ThreadAffinityWorkload
		{
			Thread::ThreadId start_thread = 0;
			Thread::ThreadId first_tick_thread = 0;
			AtomicValue<int> tick_count{0};

			void start(float) { start_thread = Thread::get_current_thread_id(); }

			void tick(const TickInfo&)
			{
				const int previous = tick_count.fetch_add(1);
				if (previous == 0)
				{
					first_tick_thread = Thread::get_current_thread_id();
				}
			}
		};
		ROBOTICK_REGISTER_WORKLOAD(ThreadAffinityWorkload)

		enum class LayoutTestEnum : uint32_t
		{
			Alpha = 0,
			Beta = 1,
			Gamma = 2
		};
		ROBOTICK_REGISTER_ENUM_BEGIN(LayoutTestEnum)
		ROBOTICK_ENUM_VALUE("Alpha", LayoutTestEnum::Alpha)
		ROBOTICK_ENUM_VALUE("Beta", LayoutTestEnum::Beta)
		ROBOTICK_ENUM_VALUE("Gamma", LayoutTestEnum::Gamma)
		ROBOTICK_REGISTER_ENUM_END(LayoutTestEnum)

		struct LayoutEnumConfig
		{
			LayoutTestEnum status = LayoutTestEnum::Alpha;
		};
		ROBOTICK_REGISTER_STRUCT_BEGIN(LayoutEnumConfig)
		ROBOTICK_STRUCT_FIELD(LayoutEnumConfig, LayoutTestEnum, status)
		ROBOTICK_REGISTER_STRUCT_END(LayoutEnumConfig)

		struct LayoutEnumWorkload
		{
			LayoutEnumConfig config;
		};
		ROBOTICK_REGISTER_WORKLOAD(LayoutEnumWorkload, LayoutEnumConfig)

	} // namespace

	// === Utility helpers ===

	uint16_t find_free_port_for_test()
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

		const uint16_t port = ntohs(addr.sin_port);
		::close(sock);
		// NOTE: this is still vulnerable to a TOCTOU race—the port is released before the Engine binds, so another
		// process could grab it. We accept occasional flake on busy hosts, or keep extending the search loop if needed.
		return port;
	}

	uint16_t choose_telemetry_port()
	{
		for (int attempt = 0; attempt < 3; ++attempt)
		{
			const uint16_t port = find_free_port_for_test();
			if (port != 0)
				return port;
		}
		ROBOTICK_FATAL_EXIT("Engine test: no free telemetry port found after 3 attempts");
		return 0;
	}

	bool bind_to_port(uint16_t port)
	{
		int sock = ::socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0)
			return false;

		int reuse = 1;
		::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		addr.sin_port = htons(port);

		bool success = (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
		::close(sock);
		return success;
	}

	// === Tests ===

	TEST_CASE("Unit/Framework/Engine")
	{
		SECTION("DummyWorkload stores tick rate correctly")
		{
			Model model;
			model.set_telemetry_port(choose_telemetry_port());
			static const WorkloadSeed workload_seed{
				TypeId("DummyWorkload"),
				StringView("A"),
				123.0f,
				{}, // children
				{}, // config
				{}	// inputs
			};
			static const WorkloadSeed* const workloads[] = {&workload_seed};
			model.use_workload_seeds(workloads);
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			float tick_rate = engine.get_root_instance_info()->seed->tick_rate_hz;
			REQUIRE(tick_rate == Catch::Approx(123.0f));
		}

		SECTION("DummyWorkload config is loaded via load()")
		{
			Model model;
			model.set_telemetry_port(choose_telemetry_port());
			static const FieldConfigEntry inputs[] = {{"input_string_64", "hello there"}, {"input_float", "1.234"}};
			static const WorkloadSeed workload_seed{
				TypeId("DummyWorkload"),
				StringView("A"),
				1.0f,
				{},	   // children
				{},	   // config
				inputs // inputs
			};
			static const WorkloadSeed* const workloads[] = {&workload_seed};
			model.use_workload_seeds(workloads);
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			const DummyWorkload* ptr = engine.find_instance<DummyWorkload>(workload_seed.unique_name);
			REQUIRE(ptr->inputs.input_float == 1.234f);
			REQUIRE(ptr->inputs.input_string_64 == "hello there");
		}

		SECTION("DummyWorkload inputs are loaded via load()")
		{
			Model model;
			model.set_telemetry_port(choose_telemetry_port());
			static const FieldConfigEntry config[] = {{"value", "42"}};
			static const WorkloadSeed workload_seed{
				TypeId("DummyWorkload"),
				StringView("A"),
				1.0f,
				{}, // children
				config,
				{} // inputs
			};
			static const WorkloadSeed* const workloads[] = {&workload_seed};
			model.use_workload_seeds(workloads);
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			const DummyWorkload* ptr = engine.find_instance<DummyWorkload>(workload_seed.unique_name);
			REQUIRE(ptr->config.value == 42);
		}

		SECTION("Multiple workloads supported")
		{
			Model model;
			model.set_telemetry_port(choose_telemetry_port());
			static const FieldConfigEntry config_one[] = {{"value", "1"}};
			static const FieldConfigEntry config_two[] = {{"value", "2"}};
			static const WorkloadSeed workload_one{
				TypeId("DummyWorkload"),
				StringView("one"),
				1.0f,
				{},			// children
				config_one, // config
				{}			// inputs
			};
			static const WorkloadSeed workload_two{
				TypeId("DummyWorkload"),
				StringView("two"),
				1.0f,
				{},			// children
				config_two, // config
				{}			// inputs
			};
			static const WorkloadSeed* const root_children[] = {&workload_one, &workload_two};
			static const WorkloadSeed root{
				TypeId("TestSequencedGroupWorkload"),
				StringView("group"),
				1.0f,
				root_children,
				{}, // config
				{}	// inputs
			};
			static const WorkloadSeed* const workloads[] = {&workload_one, &workload_two, &root};
			model.use_workload_seeds(workloads);
			model.set_root_workload(root);

			Engine engine;
			engine.load(model);

			const DummyWorkload* one = engine.find_instance<DummyWorkload>("one");
			const DummyWorkload* two = engine.find_instance<DummyWorkload>("two");

			REQUIRE(one->config.value == 1);
			REQUIRE(two->config.value == 2);
		}

		SECTION("Workloads receive tick call")
		{
			Model model;
			model.set_telemetry_port(choose_telemetry_port());
			static const WorkloadSeed workload_seed{
				TypeId("TickCounterWorkload"),
				StringView("ticky"),
				200.0f,
				{}, // children
				{}, // config
				{}	// inputs
			};
			static const WorkloadSeed* const workloads[] = {&workload_seed};
			model.use_workload_seeds(workloads);
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			AtomicFlag stop_after_next_tick_flag{true};
			engine.run(stop_after_next_tick_flag); // will tick at least once even if stop_after_next_tick_flag is true

			const TickCounterWorkload* ptr = engine.find_instance<TickCounterWorkload>(workload_seed.unique_name);
			REQUIRE(ptr->count >= 1);
		}

		SECTION("Overrun workload increments counter")
		{
			Model model;
			model.set_telemetry_port(choose_telemetry_port());
			static const WorkloadSeed workload_seed{
				TypeId("OverrunWorkload"),
				StringView("overrun"),
				1000.0f,
				{}, // children
				{}, // config
				{}	// inputs
			};
			static const WorkloadSeed* const workloads[] = {&workload_seed};
			model.use_workload_seeds(workloads);
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			AtomicFlag stop_after_next_tick_flag{false};
			EngineRunThread runner(engine, stop_after_next_tick_flag);

			Thread::sleep_ms(50);
			stop_after_next_tick_flag.set();

			const WorkloadInstanceInfo* root_info = engine.get_root_instance_info();
			REQUIRE(root_info != nullptr);
			REQUIRE(root_info->workload_stats != nullptr);
			CHECK(root_info->workload_stats->overrun_count > 0);
		}

		SECTION("Telemetry + remote lifecycle")
		{
			const uint16_t telemetry_port = choose_telemetry_port();
			REQUIRE(telemetry_port != 0);

			auto run_engine_once = [&](uint16_t port)
			{
				Model model;
				model.set_telemetry_port(port);
				static const WorkloadSeed workload_seed{
					TypeId("TickCounterWorkload"),
					StringView("telemetry_ticky"),
					200.0f,
					{}, // children
					{}, // config
					{}	// inputs
				};
				static const WorkloadSeed* const workloads[] = {&workload_seed};
				model.use_workload_seeds(workloads);
				model.set_root_workload(workload_seed);

				Engine engine;
				engine.load(model);

				AtomicFlag stop_flag{false};
				EngineRunThread runner(engine, stop_flag);

				Thread::sleep_ms(30);
				stop_flag.set();
			};

			run_engine_once(telemetry_port);
			REQUIRE(bind_to_port(telemetry_port));
			run_engine_once(telemetry_port);
		}

		SECTION("Telemetry layout emits enum metadata")
		{
			Model model;
			const uint16_t telemetry_port = choose_telemetry_port();
			model.set_telemetry_port(telemetry_port);
			static const WorkloadSeed layout_workload_seed{TypeId("LayoutEnumWorkload"), StringView("layout_enum"), 30.0f, {}, {}, {}};
			static const WorkloadSeed root_workload_seed{TypeId("TickCounterWorkload"), StringView("layout_root"), 30.0f, {}, {}, {}};
			static const WorkloadSeed* const workloads[] = {&layout_workload_seed, &root_workload_seed};
			model.use_workload_seeds(workloads);
			model.set_root_workload(root_workload_seed);

			Engine engine;
			engine.load(model);
			AtomicFlag stop_flag{false};
			EngineRunThread runner(engine, stop_flag);

			char url[256];
			::snprintf(url, sizeof(url), "http://127.0.0.1:%u/api/telemetry/workloads_buffer/layout", static_cast<unsigned int>(telemetry_port));
			Thread::sleep_ms(50);
			const HttpResponse layout_response = http_request(url);
			REQUIRE(layout_response.status_code == 200);

			json::Document layout_document;
			REQUIRE(layout_document.parse(layout_response.body.data));
			const json::Value layout = layout_document.root();
			REQUIRE(layout.contains("types"));
			const json::Value types = layout["types"];
			REQUIRE(types.is_array());

			json::Value enum_it;
			types.for_each_array(
				[&](const json::Value type_json)
				{
					if (!enum_it.is_valid() && type_json.contains("name") && type_json["name"].equals("LayoutTestEnum"))
					{
						enum_it = type_json;
					}
				});
			REQUIRE(enum_it.is_valid());

			REQUIRE(enum_it.contains("enum_values"));
			const json::Value enum_values = enum_it["enum_values"];
			REQUIRE(enum_values.size() == 3);
			CHECK(enum_values[static_cast<size_t>(0)]["name"].equals("Alpha"));
			CHECK(enum_values[static_cast<size_t>(0)]["value"].get_int64() == 0);
			CHECK(enum_values[static_cast<size_t>(1)]["name"].equals("Beta"));
			CHECK(enum_values[static_cast<size_t>(1)]["value"].get_int64() == 1);
			CHECK(enum_values[static_cast<size_t>(2)]["name"].equals("Gamma"));
			CHECK(enum_values[static_cast<size_t>(2)]["value"].get_int64() == 2);

			CHECK(enum_it["enum_underlying_size"].get_int64() == sizeof(LayoutTestEnum));
			CHECK(enum_it["enum_is_flags"].get_bool() == false);

			stop_flag.set();
		}

		SECTION("start_fn executes on same thread as tick_fn")
		{
			Model model;
			model.set_telemetry_port(choose_telemetry_port());
			static const WorkloadSeed workload_seed{
				TypeId("ThreadAffinityWorkload"),
				StringView("affinity"),
				120.0f,
				{}, // children
				{}, // config
				{}	// inputs
			};
			static const WorkloadSeed* const workloads[] = {&workload_seed};
			model.use_workload_seeds(workloads);
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			AtomicFlag stop_flag{false};
			EngineRunThread runner(engine, stop_flag);

			Thread::sleep_ms(30);
			stop_flag.set();

			const auto* info = engine.find_instance<ThreadAffinityWorkload>(workload_seed.unique_name);
			REQUIRE(info != nullptr);
			REQUIRE(info->tick_count.load() > 0);
			CHECK(info->start_thread == info->first_tick_thread);
			CHECK(info->start_thread != Thread::ThreadId{});
		}

		SECTION("Telemetry gateway routes local and peer telemetry")
		{
			const uint16_t peer_port = choose_telemetry_port();
			const uint16_t gateway_port = choose_telemetry_port();
			REQUIRE(peer_port != 0);
			REQUIRE(gateway_port != 0);
			REQUIRE(peer_port != gateway_port);

			Model peer_model;
			peer_model.set_model_name("peer-model");
			peer_model.set_telemetry_port(peer_port);
			static const FieldConfigEntry peer_inputs[] = {{"input_float", "1.0"}, {"input_string_64", "hello"}};
			static const WorkloadSeed peer_root{TypeId("TickCounterWorkload"), StringView("peer_root"), 30.0f, {}, {}, {}};
			static const WorkloadSeed peer_workload{TypeId("DummyWorkload"), StringView("peer_dummy"), 30.0f, {}, {}, peer_inputs};
			static const WorkloadSeed* const peer_workloads[] = {&peer_workload, &peer_root};
			peer_model.use_workload_seeds(peer_workloads);
			peer_model.set_root_workload(peer_root);

			Model gateway_model;
			gateway_model.set_model_name("gateway-model");
			gateway_model.set_telemetry_port(gateway_port);
			gateway_model.set_telemetry_is_gateway(true);
			static const WorkloadSeed gateway_workload{TypeId("TickCounterWorkload"), StringView("gateway_counter"), 30.0f, {}, {}, {}};
			static const WorkloadSeed gateway_root{TypeId("TickCounterWorkload"), StringView("gateway_root"), 30.0f, {}, {}, {}};
			static const WorkloadSeed* const gateway_workloads[] = {&gateway_workload, &gateway_root};
			static const TelemetryPeerSeed telemetry_peer{StringView("peer-model"), StringView("127.0.0.1"), peer_port, false};
			static const TelemetryPeerSeed* const gateway_peers[] = {&telemetry_peer};
			gateway_model.use_workload_seeds(gateway_workloads);
			gateway_model.use_telemetry_peer_seeds(gateway_peers);
			gateway_model.set_root_workload(gateway_root);

			Engine peer_engine;
			peer_engine.load(peer_model);
			AtomicFlag peer_stop{false};
			EngineRunThread peer_runner(peer_engine, peer_stop);

			Engine gateway_engine;
			gateway_engine.load(gateway_model);
			AtomicFlag gateway_stop{false};
			EngineRunThread gateway_runner(gateway_engine, gateway_stop);

			Thread::sleep_ms(100);

			char url[256];
			::snprintf(url, sizeof(url), "http://127.0.0.1:%u/api/telemetry-gateway/models", static_cast<unsigned int>(gateway_port));
			const HttpResponse models_response = http_request(url);
			REQUIRE(models_response.status_code == 200);
			json::Document models_document;
			REQUIRE(models_document.parse(models_response.body.data));
			const json::Value models_json = models_document.root();
			REQUIRE(models_json.is_object());
			REQUIRE(models_json.contains("models"));
			const json::Value models = models_json["models"];
			bool saw_gateway_model = false;
			bool saw_peer_model = false;
			models.for_each_array(
				[&](const json::Value model_json)
				{
					if (model_json["model_id"].equals("gateway-model"))
					{
						saw_gateway_model = true;
					}
					if (model_json["model_id"].equals("peer-model"))
					{
						saw_peer_model = true;
					}
				});
			REQUIRE(saw_gateway_model);
			REQUIRE(saw_peer_model);

			::snprintf(url,
				sizeof(url),
				"http://127.0.0.1:%u/api/telemetry-gateway/peer-model/workloads_buffer/layout",
				static_cast<unsigned int>(gateway_port));
			const HttpResponse layout_response = http_request(url);
			REQUIRE(layout_response.status_code == 200);
			json::Document layout_document;
			REQUIRE(layout_document.parse(layout_response.body.data));
			const json::Value layout_json = layout_document.root();
			REQUIRE(layout_json.is_object());
			REQUIRE(layout_json.contains("engine_session_id"));
			REQUIRE(layout_json.contains("writable_inputs"));

			const json::Value writable_inputs = layout_json["writable_inputs"];
			REQUIRE(writable_inputs.is_array());
			json::Value target_float_writable;
			json::Value target_string_writable;
			writable_inputs.for_each_array(
				[&](const json::Value writable)
				{
					if (!target_float_writable.is_valid() && writable.contains("field_path") &&
						contains_text(writable["field_path"].get_c_string(), "input_float"))
					{
						target_float_writable = writable;
					}
					if (!target_string_writable.is_valid() && writable.contains("field_path") &&
						contains_text(writable["field_path"].get_c_string(), "input_string_64"))
					{
						target_string_writable = writable;
					}
				});
			REQUIRE(target_float_writable.is_valid());
			REQUIRE(target_string_writable.is_valid());

			char body[512];
			::snprintf(body,
				sizeof(body),
				"{\"engine_session_id\":\"%s\",\"writes\":[{\"field_handle\":%u,\"value\":42.25},{\"field_handle\":%u,\"value\":\"updated via "
				"batch\"}]}",
				layout_json["engine_session_id"].get_c_string(),
				static_cast<unsigned>(target_float_writable["field_handle"].get_uint64()),
				static_cast<unsigned>(target_string_writable["field_handle"].get_uint64()));
			::snprintf(url,
				sizeof(url),
				"http://127.0.0.1:%u/api/telemetry-gateway/peer-model/set_workload_input_fields_data",
				static_cast<unsigned int>(gateway_port));
			const HttpResponse write_response = http_request(url, "POST", body);
			REQUIRE(write_response.status_code == 200);

			Thread::sleep_ms(50);
			const DummyWorkload* peer_instance = peer_engine.find_instance<DummyWorkload>("peer_dummy");
			REQUIRE(peer_instance != nullptr);
			CHECK(peer_instance->inputs.input_float == Catch::Approx(42.25f));
			CHECK(peer_instance->inputs.input_string_64 == "updated via batch");

			::snprintf(body,
				sizeof(body),
				"{\"engine_session_id\":\"%s\",\"writes\":[{\"field_handle\":%u,\"value\":11.0,\"seq\":100}]}",
				layout_json["engine_session_id"].get_c_string(),
				static_cast<unsigned>(target_float_writable["field_handle"].get_uint64()));
			const HttpResponse latest_write_response = http_request(url, "POST", body);
			REQUIRE(latest_write_response.status_code == 200);

			::snprintf(body,
				sizeof(body),
				"{\"engine_session_id\":\"%s\",\"writes\":[{\"field_handle\":%u,\"value\":-99.0,\"seq\":99}]}",
				layout_json["engine_session_id"].get_c_string(),
				static_cast<unsigned>(target_float_writable["field_handle"].get_uint64()));
			const HttpResponse stale_write_response = http_request(url, "POST", body);
			REQUIRE(stale_write_response.status_code == 200);

			Thread::sleep_ms(50);
			CHECK(peer_instance->inputs.input_float == Catch::Approx(11.0f));

			gateway_stop.set();
			peer_stop.set();
		}

		SECTION("Telemetry gateway peer route can be refreshed at runtime")
		{
			const uint16_t peer_port = choose_telemetry_port();
			const uint16_t gateway_port = choose_telemetry_port();
			const uint16_t stale_peer_port = choose_telemetry_port();
			REQUIRE(peer_port != 0);
			REQUIRE(gateway_port != 0);
			REQUIRE(stale_peer_port != 0);
			REQUIRE(peer_port != gateway_port);
			REQUIRE(peer_port != stale_peer_port);
			REQUIRE(gateway_port != stale_peer_port);

			Model peer_model;
			peer_model.set_model_name("peer-model");
			peer_model.set_telemetry_port(peer_port);
			static const WorkloadSeed peer_root{TypeId("TickCounterWorkload"), StringView("peer_root"), 30.0f, {}, {}, {}};
			static const WorkloadSeed* const peer_workloads[] = {&peer_root};
			peer_model.use_workload_seeds(peer_workloads);
			peer_model.set_root_workload(peer_root);

			Model gateway_model;
			gateway_model.set_model_name("gateway-model");
			gateway_model.set_telemetry_port(gateway_port);
			gateway_model.set_telemetry_is_gateway(true);
			static const WorkloadSeed gateway_root{TypeId("TickCounterWorkload"), StringView("gateway_root"), 30.0f, {}, {}, {}};
			static const WorkloadSeed* const gateway_workloads[] = {&gateway_root};
			static const TelemetryPeerSeed stale_peer{StringView("peer-model"), StringView("127.0.0.1"), stale_peer_port, false};
			static const TelemetryPeerSeed* const gateway_peers[] = {&stale_peer};
			gateway_model.use_workload_seeds(gateway_workloads);
			gateway_model.use_telemetry_peer_seeds(gateway_peers);
			gateway_model.set_root_workload(gateway_root);

			Engine peer_engine;
			peer_engine.load(peer_model);
			AtomicFlag peer_stop{false};
			EngineRunThread peer_runner(peer_engine, peer_stop);

			Engine gateway_engine;
			gateway_engine.load(gateway_model);
			AtomicFlag gateway_stop{false};
			EngineRunThread gateway_runner(gateway_engine, gateway_stop);

			Thread::sleep_ms(100);

			char url[256];
			::snprintf(url,
				sizeof(url),
				"http://127.0.0.1:%u/api/telemetry-gateway/peer-model/workloads_buffer/layout",
				static_cast<unsigned int>(gateway_port));
			const HttpResponse stale_response = http_request(url);
			REQUIRE(stale_response.status_code == 503);

			gateway_engine.get_telemetry_server().update_peer_route("peer-model", "127.0.0.1", peer_port, false);

			const HttpResponse refreshed_response = http_request(url);
			REQUIRE(refreshed_response.status_code == 200);

			gateway_stop.set();
			peer_stop.set();
		}
	}

} // namespace robotick::test
