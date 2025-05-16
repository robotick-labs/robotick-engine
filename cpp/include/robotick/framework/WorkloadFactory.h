#pragma once

#include "robotick/framework/WorkloadRegistry.h"
#include "robotick/framework/api.h"
#include <any>
#include <cstdint>
#include <map>
#include <vector>

namespace robotick
{

    struct WorkloadHandle
    {
        uint32_t index;
    };

    struct WorkloadInstance
    {
        void *ptr;
        const WorkloadRegistryEntry *type;
        double tick_rate_hz;
    };

    class ROBOTICK_API WorkloadFactory
    {
      public:
        WorkloadFactory();
        ~WorkloadFactory();

        WorkloadHandle add_by_type(const std::string &type_name, const std::string &name,
                                   const std::map<std::string, std::any> &config);
        void finalise();
        bool is_finalised() const { return m_finalised; }

        void *get_raw_ptr(WorkloadHandle h) const;

        const char *get_type_name(WorkloadHandle h) const;

        const std::vector<WorkloadInstance> &get_all() const;

      private:
        struct Pending
        {
            const WorkloadRegistryEntry *type;
            std::map<std::string, std::any> config;
        };

        std::vector<Pending> m_pending;
        std::vector<WorkloadInstance> m_instances;

        uint8_t *m_buffer = nullptr;
        size_t m_buffer_size = 0;
        bool m_finalised = false;
    };

} // namespace robotick
