// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/config/AssertUtils.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/concurrency/Atomic.h"
#include "robotick/framework/concurrency/Thread.h"

#include <arpa/inet.h>
#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <netinet/in.h>
#include <sys/socket.h>

namespace robotick::test
{
	namespace
	{
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
			const WorkloadSeed& workload_seed = model.add("DummyWorkload", "A").set_tick_rate_hz(123.0f);
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
			const WorkloadSeed& workload_seed = model.add("DummyWorkload", "A").set_tick_rate_hz(1.0f).set_config({{"value", "42"}});
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			const DummyWorkload* ptr = engine.find_instance<DummyWorkload>(workload_seed.unique_name);
			REQUIRE(ptr->config.value == 42);
		}

		SECTION("DummyWorkload config is loaded via load()")
		{
			Model model;
			model.set_telemetry_port(choose_telemetry_port());
			const WorkloadSeed& workload_seed =
				model.add("DummyWorkload", "A").set_tick_rate_hz(1.0f).set_inputs({{"input_string_64", "hello there"}, {"input_float", "1.234"}});
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			const DummyWorkload* ptr = engine.find_instance<DummyWorkload>(workload_seed.unique_name);
			REQUIRE(ptr->inputs.input_float == 1.234f);
			REQUIRE(ptr->inputs.input_string_64 == "hello there");
		}

		SECTION("Multiple workloads supported")
		{
			Model model;
			model.set_telemetry_port(choose_telemetry_port());
			const WorkloadSeed& a = model.add("DummyWorkload", "one").set_tick_rate_hz(1.0f).set_config({{"value", "1"}});
			const WorkloadSeed& b = model.add("DummyWorkload", "two").set_tick_rate_hz(1.0f).set_config({{"value", "2"}});
			const WorkloadSeed& root = model.add("TestSequencedGroupWorkload", "group").set_tick_rate_hz(1.0f).set_children({&a, &b});
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
			const WorkloadSeed& workload_seed = model.add("TickCounterWorkload", "ticky").set_tick_rate_hz(200.0f);
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
			const WorkloadSeed& workload_seed = model.add("OverrunWorkload", "overrun").set_tick_rate_hz(1000.0f);
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
				const WorkloadSeed& workload_seed = model.add("TickCounterWorkload", "ticky").set_tick_rate_hz(200.0f);
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

		SECTION("start_fn executes on same thread as tick_fn")
		{
			Model model;
			model.set_telemetry_port(choose_telemetry_port());
			const WorkloadSeed& workload_seed = model.add("ThreadAffinityWorkload", "affinity").set_tick_rate_hz(120.0f);
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
	}

} // namespace robotick::test
