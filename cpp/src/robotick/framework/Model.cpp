
#include "robotick/framework/Model.h"

#include <cassert>
#include <functional>
#include <stdexcept>

namespace robotick
{
	void Model::finalize()
	{
		assert(m_root.is_valid() && "Model root must be set before validation");

		std::function<void(WorkloadHandle, double)> validate_recursively = [&](WorkloadHandle handle, double parent_tick_rate)
		{
			WorkloadSeed& seed = m_configs[handle.index];

			// Inherit tick rate if using TICK_RATE_FROM_PARENT (tick_rate_hz==0.0)
			if (seed.tick_rate_hz == TICK_RATE_FROM_PARENT)
			{
				seed.tick_rate_hz = parent_tick_rate;
			}
			else if (seed.tick_rate_hz > parent_tick_rate)
			{
				throw std::runtime_error("Child workload cannot have faster tick rate than parent");
			}
			// Recurse through children
			for (WorkloadHandle child : seed.children)
			{
				validate_recursively(child, seed.tick_rate_hz);
			}
		};

		// Root can run at any tick rate, but must be explicitly set
		WorkloadSeed& root_seed = m_configs[m_root.index];
		if (root_seed.tick_rate_hz == TICK_RATE_FROM_PARENT)
		{
			throw std::runtime_error("Root workload must have an explicit tick rate");
		}

		validate_recursively(m_root, root_seed.tick_rate_hz);
	}

} // namespace robotick
