#include "robotick/framework/registry/WorkloadFactory.h"
#include "robotick/framework/registry/FieldUtils.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace robotick
{
	WorkloadFactory::WorkloadFactory() = default;

	WorkloadFactory::~WorkloadFactory()
	{
		if (m_buffer)
			std::free(m_buffer);
	}

	WorkloadHandle WorkloadFactory::add_by_type(const std::string &type_name, const std::string &name,
												const double tick_rate_hz,
												const std::map<std::string, std::any> &config)
	{
		if (m_finalised)
			throw std::runtime_error("Factory already finalised");
		const auto *entry = get_workload_registry_entry(type_name);
		if (!entry)
			throw std::runtime_error("Unknown workload type: " + type_name);
		m_pending.push_back({entry, name, tick_rate_hz, config});
		return {static_cast<uint32_t>(m_pending.size() - 1)};
	}

	const std::vector<WorkloadInstance> &WorkloadFactory::get_all() const
	{
		return m_instances;
	}

	void WorkloadFactory::finalise()
	{
		if (m_finalised)
			throw std::runtime_error("Already finalised");

		// Size and align
		size_t offset = 0;
		std::vector<size_t> offsets;
		for (const auto &p : m_pending)
		{
			size_t aligned = (offset + p.type->alignment - 1) & ~(p.type->alignment - 1);
			offsets.push_back(aligned);
			offset = aligned + p.type->size;
		}

		m_buffer_size = offset;

#ifdef _MSC_VER
		m_buffer = static_cast<uint8_t *>(_aligned_malloc(m_buffer_size, 64));
#else
		m_buffer = static_cast<uint8_t *>(std::aligned_alloc(64, m_buffer_size));
#endif

		if (!m_buffer)
			throw std::bad_alloc();

		for (size_t i = 0; i < m_pending.size(); ++i)
		{
			const auto &p = m_pending[i];
			void *ptr = &m_buffer[offsets[i]];
			p.type->construct(ptr);
			if (p.type->config_struct)
				apply_struct_fields(static_cast<uint8_t *>(ptr) + p.type->config_offset, *p.type->config_struct,
									p.config);

			m_instances.push_back({ptr, p.type, p.name, p.tick_rate_hz});
		}

		m_pending.clear();
		m_finalised = true;
	}

	void *WorkloadFactory::get_raw_ptr(WorkloadHandle h) const
	{
		return m_instances.at(h.index).ptr;
	}

	const char *WorkloadFactory::get_type_name(WorkloadHandle h) const
	{
		return m_instances.at(h.index).type->name.c_str();
	}

} // namespace robotick
