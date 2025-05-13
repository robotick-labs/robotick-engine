// ===============================
// SyncedPairWorkload.cpp
// ===============================
#include "robotick/framework/SyncedPairWorkload.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <atomic>

namespace robotick
{

    struct SyncedPairWorkload::Impl
    {
        struct Buffer
        {
            InputBlock in;
            OutputBlock out;
        } buffers[2];

        std::shared_ptr<IWorkload> primary;
        std::shared_ptr<IWorkload> secondary;

        std::thread secondary_thread;
        std::atomic<bool> stop = false;
        std::atomic<int> active_slot = 0;

        std::condition_variable cv;
        std::mutex mutex;
        bool ready = false;
    };

    SyncedPairWorkload::SyncedPairWorkload(std::string name,
                                           std::shared_ptr<IWorkload> primary,
                                           std::shared_ptr<IWorkload> secondary)
        : WorkloadBase(std::move(name)), m_impl(std::make_unique<Impl>())
    {
        m_impl->primary = std::move(primary);
        m_impl->secondary = std::move(secondary);
    }

    SyncedPairWorkload::~SyncedPairWorkload()
    {
        stop();
    }

    double SyncedPairWorkload::get_tick_rate_hz()
    {
        return m_impl->primary->get_tick_rate_hz();
    }

    void SyncedPairWorkload::load()
    {
        m_impl->primary->load();
        m_impl->secondary->load();
    }

    void SyncedPairWorkload::setup()
    {
        m_impl->primary->setup();
        m_impl->secondary->setup();

        m_impl->secondary_thread = std::thread([this]()
                                               {
            while (!m_impl->stop) {
                std::unique_lock<std::mutex> lock(m_impl->mutex);
                m_impl->cv.wait(lock, [this]() { return m_impl->ready || m_impl->stop; });
                m_impl->ready = false;
                lock.unlock();

                if (m_impl->stop) break;

                int inactive_slot = 1 - m_impl->active_slot;
                InputBlock in = m_impl->buffers[inactive_slot].in;
                OutputBlock out;

                m_impl->secondary->tick(in, out);
                m_impl->buffers[inactive_slot].out = out;
            } });
    }

    void SyncedPairWorkload::tick(const InputBlock &in, OutputBlock &out)
    {
        m_impl->buffers[m_impl->active_slot].in = in;

        {
            std::lock_guard<std::mutex> lock(m_impl->mutex);
            m_impl->ready = true;
        }
        m_impl->cv.notify_one();

        m_impl->primary->tick(in, out);
        m_impl->active_slot = 1 - m_impl->active_slot;
    }

    void SyncedPairWorkload::stop()
    {
        if (!m_impl || m_impl->stop.exchange(true))
            return;

        {
            std::lock_guard<std::mutex> lock(m_impl->mutex);
            m_impl->ready = true;
        }
        m_impl->cv.notify_one();
        if (m_impl->secondary_thread.joinable())
        {
            m_impl->secondary_thread.join();
        }
    }

}
