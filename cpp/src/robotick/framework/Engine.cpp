#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/utils/Thread.h"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

namespace robotick
{

	namespace
	{
		// Platform helpers
		void enable_high_resolution_timing()
		{
#ifdef _WIN32
			timeBeginPeriod(1);
#else
			// No-op on Linux
#endif
		}

		void disable_high_resolution_timing()
		{
#ifdef _WIN32
			timeEndPeriod(1);
#else
			// No-op on Linux
#endif
		}

		void set_thread_priority_high()
		{
#ifdef _WIN32
			SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#else
			// Optional: use pthread_setschedparam if real-time priority is
			// needed
#endif
		}

		void set_thread_affinity(int core)
		{
#ifdef _WIN32
			DWORD_PTR mask = 1 << core;
			SetThreadAffinityMask(GetCurrentThread(), mask);
#else
			// Optional: use pthread_setaffinity_np
#endif
		}
	} // namespace

	struct Engine::Impl
	{
		std::vector<std::thread> threads;
		std::atomic<bool> stop_flag = false;
		const Model *model = nullptr;
	};

	Engine::Engine() : m_impl(std::make_unique<Impl>())
	{
	}

	Engine::~Engine()
	{
		m_impl->stop_flag = true;
		for (auto &t : m_impl->threads)
		{
			if (t.joinable())
				t.join();
		}
	}

	void Engine::load(const Model &model)
	{
		m_impl->threads.clear();
		m_impl->model = &model;

		const auto &handles = model.get_workloads();
		const auto &instances = model.factory().get_all();

		for (auto &h : handles)
		{
			const auto &instance = instances[h.index];
			m_impl->threads.emplace_back([&]() {
				if (instance.type->pre_load)
					instance.type->pre_load(instance.ptr);
				if (instance.type->load)
					instance.type->load(instance.ptr);
			});
		}

		for (auto &t : m_impl->threads)
		{
			if (t.joinable())
				t.join();
		}

		m_impl->threads.clear();
	}

	void Engine::setup()
	{
		if (!m_impl->model)
			throw std::runtime_error("Engine::setup called before load()");

		const auto &factory = m_impl->model->factory();
		for (auto &h : m_impl->model->get_workloads())
		{
			const auto &instance = factory.get_all()[h.index];
			if (instance.type->setup)
				instance.type->setup(instance.ptr);
		}

		enable_high_resolution_timing();
	}

	void hybrid_sleep_until(std::chrono::steady_clock::time_point target_time)
	{
		using namespace std::chrono;
		constexpr auto coarse_margin = 500us;
		constexpr auto coarse_step = 100us;

		auto now = steady_clock::now();
		while (now < target_time - coarse_margin)
		{
			std::this_thread::sleep_for(coarse_step);
			now = steady_clock::now();
		}

		while (steady_clock::now() < target_time)
		{
			// spin
		}
	}

	void Engine::start()
	{
		if (!m_impl->model)
			throw std::runtime_error("Engine::start called before load()");

		const auto &handles = m_impl->model->get_workloads();
		const auto &instances = m_impl->model->factory().get_all();

		m_impl->stop_flag = false;

		for (auto &h : handles)
		{
			const auto &instance = instances[h.index];

			m_impl->threads.emplace_back([this, &instance]() {
				set_thread_affinity(2);
				set_thread_priority_high();
				set_thread_name("robotick_" + std::string(instance.type->name));

				double hz = instance.tick_rate_hz;
				if (hz <= 0 || !instance.type->tick)
					return;

				using namespace std::chrono;
				const auto tick_interval = duration<double>(1.0 / hz);
				auto next_tick_time = steady_clock::now() + tick_interval;
				auto last_time = steady_clock::now();

				while (!m_impl->stop_flag)
				{
					auto now = steady_clock::now();
					double time_delta = duration<double>(now - last_time).count();
					last_time = now;

					instance.type->tick(instance.ptr, time_delta);
					next_tick_time += tick_interval;

					hybrid_sleep_until(time_point_cast<steady_clock::duration>(next_tick_time));
				}
			});
		}
	}

	void Engine::stop()
	{
		if (!m_impl->model)
			throw std::runtime_error("Engine::stop called before load()");

		m_impl->stop_flag = true;

		const auto &factory = m_impl->model->factory();
		for (auto &h : m_impl->model->get_workloads())
		{
			const auto &instance = factory.get_all()[h.index];
			if (instance.type->stop)
				instance.type->stop(instance.ptr);
		}

		for (auto &t : m_impl->threads)
		{
			if (t.joinable())
				t.join();
		}

		m_impl->threads.clear();

		disable_high_resolution_timing();
	}

} // namespace robotick
