// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"

#include "robotick/api.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/RemoteEngineConnection.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/registry/FieldUtils.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/TypeId.h"
#include "robotick/platform/Threading.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <future>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace robotick
{
	struct Engine::State
	{
		Model m_loaded_model;
		bool is_running = false;

		WorkloadsBuffer workloads_buffer;

		const WorkloadInstanceInfo* root_instance = nullptr;
		std::vector<WorkloadInstanceInfo> instances;
		std::unordered_map<std::string, WorkloadInstanceInfo*> instances_by_unique_name;
		std::vector<DataConnectionInfo> data_connections_all;
		std::vector<size_t> data_connections_acquired_indices;

		std::vector<std::unique_ptr<RemoteEngineConnection>> remote_engine_senders; // one sender per engine we need to send data to
		RemoteEngineConnection remote_engines_receiver;								// one receiver in case any engines need to send data to us
	};

	Engine::Engine() : state(std::make_unique<Engine::State>())
	{
	}

	Engine::~Engine()
	{
		for (auto& instance : state->instances)
		{
			if (instance.type->destruct)
			{
				void* instance_ptr = instance.get_ptr(*this);
				ROBOTICK_ASSERT(instance_ptr != nullptr);
				instance.type->destruct(instance_ptr);
			}
		}
	}

	const WorkloadInstanceInfo* Engine::get_root_instance_info() const
	{
		return state->root_instance;
	}

	const WorkloadInstanceInfo& Engine::get_instance_info(size_t index) const
	{
		return state->instances.at(index);
	}

	const WorkloadInstanceInfo* Engine::find_instance_info(const char* unique_name) const
	{
		// make it use our state->instances_by_unique_name map instead
		auto it = state->instances_by_unique_name.find(unique_name);
		return (it != state->instances_by_unique_name.end()) ? it->second : nullptr;
	}

	const std::vector<WorkloadInstanceInfo>& Engine::get_all_instance_info() const
	{
		return state->instances;
	}

	const std::vector<DataConnectionInfo>& Engine::get_all_data_connections() const
	{
		return state->data_connections_all;
	}

	WorkloadsBuffer& Engine::get_workloads_buffer() const
	{
		return state->workloads_buffer;
	}

	size_t Engine::compute_blackboard_memory_requirements(const std::vector<WorkloadInstanceInfo>& instances)
	{
		size_t total = 0;
		for (const auto& instance : instances)
		{
			const auto* type = instance.type;
			if (!type)
				continue;

			void* instance_ptr = instance.get_ptr(*this);
			ROBOTICK_ASSERT(instance_ptr != nullptr);

			auto accumulate = [&](const StructRegistryEntry* struct_info)
			{
				if (!struct_info)
					return;
				const size_t struct_offset = struct_info->offset_within_workload;
				const auto struct_ptr = static_cast<uint8_t*>(instance_ptr) + struct_offset;

				for (const FieldInfo& field : struct_info->fields)
				{
					if (field.type == GET_TYPE_ID(Blackboard))
					{
						ROBOTICK_ASSERT(field.offset_within_struct != OFFSET_UNBOUND);
						const auto* blackboard = reinterpret_cast<const Blackboard*>(struct_ptr + field.offset_within_struct);
						total += blackboard->get_info()->total_datablock_size;
					}
				}
			};

			accumulate(type->config_struct);
			accumulate(type->input_struct);
			accumulate(type->output_struct);
		}
		return total;
	}

	void Engine::bind_blackboards_in_struct(WorkloadInstanceInfo& instance, const StructRegistryEntry& struct_entry, size_t& offset)
	{
		for (const FieldInfo& field : struct_entry.fields)
		{
			if (field.type == GET_TYPE_ID(Blackboard))
			{
				Blackboard& blackboard = field.get_data<Blackboard>(state->workloads_buffer, instance, struct_entry);
				blackboard.bind(offset);
				offset += blackboard.get_info()->total_datablock_size;
			}
		}
	}

	void Engine::bind_blackboards_for_instances(std::vector<WorkloadInstanceInfo>& instances, size_t start_offset)
	{
		for (auto& instance : instances)
		{
			const auto* type = instance.type;
			if (!type)
				continue;

			if (type->config_struct)
				bind_blackboards_in_struct(instance, *type->config_struct, start_offset);
			if (type->input_struct)
				bind_blackboards_in_struct(instance, *type->input_struct, start_offset);
			if (type->output_struct)
				bind_blackboards_in_struct(instance, *type->output_struct, start_offset);
		}
	}

	void Engine::load(const Model& model)
	{
		if (!model.get_root().is_valid())
			ROBOTICK_FATAL_EXIT("Model has no root workload");

		state->instances.clear();
		state->data_connections_all.clear();
		state->data_connections_acquired_indices.clear();
		state->root_instance = nullptr;
		state->m_loaded_model = model;

		const auto& seeds = model.get_workload_seeds();
		size_t workloads_cursor = 0;
		std::vector<size_t> offsets;
		for (const auto& seed : seeds)
		{
			const auto* type = WorkloadRegistry::get().find(seed.type.c_str());
			if (!type)
				ROBOTICK_FATAL_EXIT("Unknown workload type: %s", seed.type.c_str());

			const size_t align = std::max<size_t>(type->alignment, alignof(std::max_align_t));
			workloads_cursor = (workloads_cursor + align - 1) & ~(align - 1);
			offsets.push_back(workloads_cursor);
			workloads_cursor += type->size;
		}

		const size_t total_size = workloads_cursor;
		state->workloads_buffer = WorkloadsBuffer(total_size + DEFAULT_MAX_BLACKBOARDS_BYTES);
		uint8_t* buffer_ptr = state->workloads_buffer.raw_ptr();
		state->instances.reserve(seeds.size());
		state->instances_by_unique_name.reserve(seeds.size());

		for (size_t i = 0; i < seeds.size(); ++i)
		{
			const auto& seed = seeds[i];
			const auto* type = WorkloadRegistry::get().find(seed.type.c_str());
			uint8_t* ptr = buffer_ptr + offsets[i];

			WorkloadInstanceInfo& new_info =
				state->instances.emplace_back(WorkloadInstanceInfo{offsets[i], type, seed.name, seed.tick_rate_hz, {}, WorkloadInstanceStats{}});

			// add it to our map for quick lookup by name
			state->instances_by_unique_name[new_info.unique_name] = &new_info;

#if defined(ROBOTICK_DEBUG)
			constexpr size_t MAX_SIZE = 2048;
			alignas(std::max_align_t) static constexpr uint8_t zero[MAX_SIZE] = {};
			ROBOTICK_ASSERT(type->size <= MAX_SIZE);
			ROBOTICK_ASSERT(std::memcmp(ptr, zero, type->size) == 0);
#endif
			if (type->construct)
				type->construct(ptr);
		}

		std::vector<std::future<void>> preload_futures;
		for (size_t i = 0; i < seeds.size(); ++i)
		{
			const auto& seed = seeds[i];
			const auto* type = state->instances[i].type;
			uint8_t* ptr = buffer_ptr + state->instances[i].offset_in_workloads_buffer;

			preload_futures.emplace_back(std::async(std::launch::async,
				[=, this]()
				{
					if (type->set_engine_fn)
						type->set_engine_fn(ptr, *this);
					if (type->config_struct)
						apply_struct_fields(ptr + type->config_struct->offset_within_workload, *type->config_struct, seed.config);
					if (type->pre_load_fn)
						type->pre_load_fn(ptr);
				}));
		}
		for (auto& fut : preload_futures)
			fut.get();

		size_t blackboard_size = compute_blackboard_memory_requirements(state->instances);
		if (blackboard_size > DEFAULT_MAX_BLACKBOARDS_BYTES)
			ROBOTICK_FATAL_EXIT("Blackboard memory (%zu) exceeds max allowed (%zu)", blackboard_size, DEFAULT_MAX_BLACKBOARDS_BYTES);

		if (blackboard_size > 0)
		{
			size_t start = workloads_cursor;
			workloads_cursor += blackboard_size;
			bind_blackboards_for_instances(state->instances, start);
		}

		std::vector<std::future<void>> load_futures;
		for (auto& inst : state->instances)
		{
			void* ptr = inst.get_ptr(*this);
			const auto* type = inst.type;
			load_futures.emplace_back(std::async(std::launch::async,
				[ptr, type]()
				{
					if (type->load_fn)
						type->load_fn(ptr);
				}));
		}
		for (auto& fut : load_futures)
			fut.get();

		state->data_connections_all = DataConnectionsFactory::create(state->workloads_buffer, model.get_data_connection_seeds(), state->instances);
		std::vector<DataConnectionInfo*> pending;
		for (auto& conn : state->data_connections_all)
			pending.push_back(&conn);

		for (size_t i = 0; i < seeds.size(); ++i)
		{
			auto& inst = state->instances[i];
			const auto& seed = seeds[i];

			if (inst.type->set_children_fn)
			{
				for (auto child_handle : seed.children)
					inst.children.push_back(&get_instance_info(child_handle.index));
			}
		}

		const auto* root_instance = &get_instance_info(model.get_root().index);
		ROBOTICK_ASSERT(root_instance != nullptr);
		if (root_instance->type->set_children_fn)
			root_instance->type->set_children_fn(root_instance->get_ptr(*this), root_instance->children, pending);

		auto new_end = std::remove_if(pending.begin(), pending.end(),
			[this](DataConnectionInfo* conn)
			{
				if (conn->expected_handler == DataConnectionInfo::ExpectedHandler::ParentGroupOrEngine)
				{
					state->data_connections_acquired_indices.push_back(conn - state->data_connections_all.data());
					return true;
				}
				return false;
			});
		pending.erase(new_end, pending.end());

		if (!pending.empty())
		{
			const auto* conn = pending.front();
			ROBOTICK_FATAL_EXIT("Unclaimed connection: %s -> %s", conn->seed.source_field_path.c_str(), conn->seed.dest_field_path.c_str());
		}

		for (auto& inst : state->instances)
		{
			if (inst.type->setup_fn)
				inst.type->setup_fn(inst.get_ptr(*this));
		}

		ROBOTICK_INFO("Setting up remote engine senders...");
		setup_remote_engine_senders(model);

		ROBOTICK_INFO("Setting up remote engines listener...");
		setup_remote_engines_listener();
		ROBOTICK_INFO("Finished setting up remote engines listener");

		state->root_instance = root_instance;
	}

	struct FieldPathTokens
	{
		std::string workload;
		std::string struct_name; // "inputs" or "outputs"
		std::string field;
		std::string subfield; // optional
	};

	FieldPathTokens parse_field_path(const std::string& path)
	{
		FieldPathTokens tokens;

		size_t dot1 = path.find('.');
		if (dot1 == std::string::npos)
			return tokens;

		tokens.workload = path.substr(0, dot1);

		size_t dot2 = path.find('.', dot1 + 1);
		if (dot2 == std::string::npos)
			return tokens;

		tokens.struct_name = path.substr(dot1 + 1, dot2 - dot1 - 1);

		size_t dot3 = path.find('.', dot2 + 1);
		if (dot3 == std::string::npos)
		{
			tokens.field = path.substr(dot2 + 1);
		}
		else
		{
			tokens.field = path.substr(dot2 + 1, dot3 - dot2 - 1);
			tokens.subfield = path.substr(dot3 + 1);
		}

		return tokens;
	}

	constexpr int DEFAULT_REMOTE_ENGINE_PORT = 7262;

	void Engine::setup_remote_engines_listener()
	{
		state->remote_engines_receiver.configure(
			RemoteEngineConnection::ConnectionConfig{"", DEFAULT_REMOTE_ENGINE_PORT}, RemoteEngineConnection::Mode::Receiver);

		state->remote_engines_receiver.set_field_binder(
			[&](const std::string& path, RemoteEngineConnection::Field& out)
			{
				FieldPathTokens tokens = parse_field_path(path);
				const WorkloadInstanceInfo* found_workload = find_instance_info(tokens.workload.c_str());
				if (!found_workload || tokens.struct_name != "inputs")
				{
					return false;
				}

				const FieldInfo* found_field = found_workload->type->input_struct->find_field(tokens.field.c_str());
				if (!found_field)
				{
					return false;
				}

				// Get pointer to that field
				TypeId type = found_field->type;
				void* ptr = found_field->get_data_ptr(get_workloads_buffer(), *found_workload, *found_workload->type->input_struct);
				size_t size = found_field->size;
				if (!ptr)
				{
					ROBOTICK_FATAL_EXIT("Engine::setup_remote_engines_listener - data ptr missing %s", path.c_str());
				}

				// Handle Blackboard subfields if needed
				if (!tokens.subfield.empty())
				{
					if (type == GET_TYPE_ID(Blackboard))
					{
						Blackboard& blackboard = *static_cast<Blackboard*>(ptr);
						const BlackboardFieldInfo* sbinfo = blackboard.get_field_info(tokens.subfield);
						if (!sbinfo)
						{
							ROBOTICK_FATAL_EXIT("Engine::setup_remote_engines_listener - unknown subfield %s", path.c_str());
						}
						ptr = sbinfo->get_data_ptr(blackboard);
						type = sbinfo->type;
						size = sbinfo->size;
					}
					else
					{
						ROBOTICK_FATAL_EXIT("Remote Engine Connections to non-Blackboad subfields are not yet supported");
					}
				}

				out.path = path;
				out.recv_ptr = ptr;
				out.size = size;
				out.type_hash = type;

				return true;
			});
	}

	void Engine::setup_remote_engine_senders(const Model& model)
	{
		const auto& remote_models = model.get_remote_models();

		state->remote_engine_senders.clear(); // Just in case
		state->remote_engine_senders.reserve(remote_models.size());

		for (const auto& remote_model_entry : remote_models)
		{
			const RemoteModelSeed& remote_model = remote_model_entry.second;

			if (remote_model.remote_data_connection_seeds.size() == 0)
			{
				ROBOTICK_WARNING("Remote model '%s' has no remote data-connections - skipping adding remote_engine_sender for it",
					remote_model.model_name.c_str());

				continue;
			}

			RemoteEngineConnection& remote_engine_connection =
				*(state->remote_engine_senders.emplace_back(std::make_unique<RemoteEngineConnection>()).get());

			RemoteEngineConnection::ConnectionConfig config;
			config.host = remote_model.comms_channel;
			config.port = DEFAULT_REMOTE_ENGINE_PORT;

			remote_engine_connection.configure(config, RemoteEngineConnection::Mode::Sender);

			for (const auto& remote_data_connection_seed : remote_model.remote_data_connection_seeds)
			{
				RemoteEngineConnection::Field remote_field;
				remote_field.path = remote_data_connection_seed.dest_field_path;
				remote_field.send_ptr = nullptr; // TODO - establish local pointer from which to copy value each tick
				remote_field.size = 0;			 // TODO - find this out
				remote_field.type_hash = 0;		 // TODO - find this out too

				remote_engine_connection.register_field(remote_field);
			}
		}
	}

	bool Engine::is_running() const
	{
		return state && state->is_running;
	}

	void Engine::run(const AtomicFlag& stop_after_next_tick_flag)
	{
		const WorkloadHandle root_handle = state->m_loaded_model.get_root();
		if (root_handle.index >= state->instances.size())
			ROBOTICK_FATAL_EXIT("Invalid root workload handle");

		const auto& root_info = state->instances[root_handle.index];
		void* root_ptr = root_info.get_ptr(*this);

		if (root_info.tick_rate_hz <= 0.0)
			ROBOTICK_FATAL_EXIT("Root workload must have valid tick_rate_hz>0.0 - check your model settings");

		if (root_info.type->tick_fn == nullptr)
			ROBOTICK_FATAL_EXIT("Root workload must have valid tick_fn - check it has been correctly registered");

		if (!root_ptr)
			ROBOTICK_FATAL_EXIT("Root workload must have valid object-pointer - check it has been correctly registered");

		// Start all workloads
		for (auto& inst : state->instances)
		{
			if (inst.type->start_fn)
				inst.type->start_fn(inst.get_ptr(*this), inst.tick_rate_hz);
		}

		state->is_running = true;

		const auto child_tick_interval_sec = std::chrono::duration<double>(1.0 / root_info.tick_rate_hz);
		const auto child_tick_interval = std::chrono::duration_cast<std::chrono::steady_clock::duration>(child_tick_interval_sec);

		const auto engine_start_time = std::chrono::steady_clock::now() - child_tick_interval;
		// ^- subtract tick-interval so initial delta is from tick-interval

		auto last_tick_time = engine_start_time;
		auto next_tick_time = engine_start_time;

		TickInfo tick_info;

		do
		{
			const auto now = std::chrono::steady_clock::now();
			const auto ns_since_start = std::chrono::duration_cast<std::chrono::nanoseconds>(now - engine_start_time).count();
			const auto ns_since_last = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_tick_time).count();

			tick_info.tick_count += 1;
			tick_info.time_now_ns = ns_since_start;
			tick_info.time_now = ns_since_start * 1e-9;
			tick_info.delta_time = ns_since_last * 1e-9;

			last_tick_time = now;

			// update remote data-connections
			tick_remote_engine_connections(tick_info);

			// update local data-connections
			for (size_t index : state->data_connections_acquired_indices)
				state->data_connections_all[index].do_data_copy();

			std::atomic_thread_fence(std::memory_order_release);

			root_info.type->tick_fn(root_ptr, tick_info);

			const auto now_post = std::chrono::steady_clock::now();
			root_info.mutable_stats.last_tick_duration = std::chrono::duration<double>(now_post - now).count();
			root_info.mutable_stats.last_time_delta = tick_info.delta_time;

			next_tick_time += child_tick_interval;
			Thread::hybrid_sleep_until(next_tick_time);

		} while (!stop_after_next_tick_flag.is_set());

		state->is_running = false;

		for (auto& inst : state->instances)
		{
			if (inst.type->stop_fn)
				inst.type->stop_fn(inst.get_ptr(*this));
		}
	}

	void Engine::tick_remote_engine_connections(const TickInfo& tick_info)
	{
		state->remote_engines_receiver.tick(tick_info);

		for (auto& remote_engine_sender_ptr : state->remote_engine_senders)
		{
			remote_engine_sender_ptr->tick(tick_info);
		}
	}

} // namespace robotick
