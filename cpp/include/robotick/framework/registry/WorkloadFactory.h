#pragma once

#include "robotick/framework/api.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include <any>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace robotick
{

    struct WorkloadHandle
    {
        uint32_t index;
    };

    struct WorkloadInstance
    {
        void*                        ptr;
        const WorkloadRegistryEntry* type;
        std::string                  unique_name;
        double                       tick_rate_hz = 0.0;
    };

    class ROBOTICK_API WorkloadFactory
    {
       public:
        WorkloadFactory();
        ~WorkloadFactory();

        WorkloadHandle add(const std::string& type_name, const std::string& name, const double tick_rate_hz,
                           const std::map<std::string, std::any>& config);

        void finalise();
        bool is_finalised() const { return m_finalised; }

        void* get_raw_ptr(WorkloadHandle h) const;

        const char* get_type_name(WorkloadHandle h) const;

        const std::vector<WorkloadInstance>& get_all() const;

       private:
        struct PendingInstance
        {
            const WorkloadRegistryEntry*    type;
            std::string                     name;
            double                          tick_rate_hz = 0.0;
            std::map<std::string, std::any> config;
        };

        std::vector<PendingInstance>  m_pending_instances;
        std::vector<WorkloadInstance> m_instances;

        uint8_t* m_buffer = nullptr;
        size_t   m_buffer_size = 0;
        bool     m_finalised = false;
    };

}  // namespace robotick
