#pragma once

#include "robotick/framework/FieldRegistry.h"
#include <map>
#include <any>
#include <string>

namespace robotick
{

    struct WorkloadRegistryEntry
    {
        std::string name;
        size_t size;
        size_t alignment;

        void (*construct)(void *ptr);

        const StructRegistryEntry *config_struct = nullptr;
        size_t config_struct_offset = 0;

        // Main execution callback (called every tick interval)
        void (*tick)(void *ptr, double time_delta) = nullptr;

        // Optional lifecycle callbacks
        void (*pre_load)(void *ptr) = nullptr;           // Called before load, async-safe
        void (*load)(void *ptr) = nullptr;               // Called in load() phase (e.g. resource allocation)
        void (*setup)(void *ptr) = nullptr;              // Called once after load, for init/final wiring
        void (*start)(void *ptr) = nullptr;              // Called just before ticking begins
        void (*stop)(void *ptr) = nullptr;               // Called after ticking ends (e.g. shutdown/cleanup)
        double (*get_tick_rate_fn)(void *ptr) = nullptr; // Called on setup when the framework needs to know our fixed tick-rate
    };

    const WorkloadRegistryEntry *get_workload_registry_entry(const std::string &name);

    template <typename T>
    const WorkloadRegistryEntry *register_workload_type(const WorkloadRegistryEntry &entry);

} // namespace robotick
