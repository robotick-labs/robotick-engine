#pragma once

#include "robotick/framework/IWorkload.h"
#include <vector>
#include <memory>

namespace robotick {

    class Model {
    public:
        void add(std::shared_ptr<IWorkload> workload) {
            m_workloads.push_back(std::move(workload));
        }

        const std::vector<std::shared_ptr<IWorkload>>& get_workloads() const {
            return m_workloads;
        }

    private:
        std::vector<std::shared_ptr<IWorkload>> m_workloads;
    };

}
