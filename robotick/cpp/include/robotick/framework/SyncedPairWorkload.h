#pragma once

#include "robotick/framework/WorkloadBase.h"
#include <atomic>
#include <thread>
#include <memory>
#include <condition_variable>

namespace robotick
{
    class SyncedPairWorkload : public WorkloadBase
    {
    public:
        SyncedPairWorkload(std::string name,
                           std::shared_ptr<IWorkload> primary,
                           std::shared_ptr<IWorkload> secondary)
            : WorkloadBase(std::move(name)),
              m_primary(std::move(primary)),
              m_secondary(std::move(secondary)),
              m_stop(false),
              m_active_slot(0)
        {
        }

        ~SyncedPairWorkload() override
        {
            stop();
        }

        double get_tick_rate_hz() override
        {
            return m_primary->get_tick_rate_hz();
        }

        void load() override
        {
            m_primary->load();
            m_secondary->load();
        }

        void setup() override
        {
            m_primary->setup();
            m_secondary->setup();

            // Start secondary thread
            m_secondary_thread = std::thread([this]()
                                             { run_secondary(); });
        }

        void tick(const InputBlock &in, OutputBlock &out) override
        {
            // Write to active slot
            m_buffers[m_active_slot].in = in;

            // Notify secondary that it has new data
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_ready = true;
            }
            m_cv.notify_one();

            // Call primary workload
            m_primary->tick(in, out);

            // Swap buffers
            m_active_slot = 1 - m_active_slot;
        }

        void stop() override
        {
            if (!m_stop.exchange(true))
            {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_ready = true;
                }
                m_cv.notify_one();
                if (m_secondary_thread.joinable())
                {
                    m_secondary_thread.join();
                }
            }
        }

    protected:
        void run_secondary()
        {
            while (!m_stop)
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this]()
                          { return m_ready || m_stop; });
                m_ready = false;
                lock.unlock();

                if (m_stop)
                    break;

                int inactive_slot = 1 - m_active_slot.load();
                InputBlock in = m_buffers[inactive_slot].in;
                OutputBlock out;

                m_secondary->tick(in, out);
                m_buffers[inactive_slot].out = out;
            }
        }

    private:
        struct Buffer
        {
            InputBlock in;
            OutputBlock out;
        };

        Buffer m_buffers[2]; // ping-pong buffer
        std::atomic<bool> m_stop;
        std::atomic<int> m_active_slot;

        std::shared_ptr<IWorkload> m_primary;
        std::shared_ptr<IWorkload> m_secondary;

        std::thread m_secondary_thread;
        std::condition_variable m_cv;
        std::mutex m_mutex;
        bool m_ready = false;
    };
}
