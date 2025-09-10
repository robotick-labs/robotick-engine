// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"
#include "robotick/framework/common/ArrayView.h"
#include "robotick/framework/common/StringView.h"
#include "robotick/framework/model/DataConnectionSeed.h"
#include "robotick/framework/model/RemoteModelSeed.h"
#include "robotick/framework/model/WorkloadSeed.h"

#ifdef ROBOTICK_ENABLE_MODEL_HEAP
#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/common/HeapVector.h"
#include "robotick/framework/common/List.h"
#endif

#include <cstdint>

namespace robotick
{
	class Model
	{
	  public:
#ifdef ROBOTICK_ENABLE_MODEL_HEAP
		// dynamic modifiers and accessor(s):
		WorkloadSeed& add();
		WorkloadSeed& add(const char* type_name, const char* unique_name);
		void connect(const char* source_field_path, const char* dest_field_path);
		RemoteModelSeed& add_remote_model(const char* model_name, const char* comms_channel);

		const List<WorkloadSeed>& get_workload_seeds_storage() const { return workload_seeds_storage; }

#endif
		// non-dynamic modifiers:
		void set_model_name(const char* in_model_name) { model_name = in_model_name; }

		template <size_t N> void use_workload_seeds(const WorkloadSeed* (&in_seeds)[N]) { use_workload_seeds(in_seeds, N); }
		void use_workload_seeds(const WorkloadSeed** all_workloads, size_t num_workloads);

		template <size_t N> void use_data_connection_seeds(const DataConnectionSeed* (&in_connections)[N])
		{
			use_data_connection_seeds(in_connections, N);
		}
		void use_data_connection_seeds(const DataConnectionSeed** in_connections, size_t num_connections);

		template <size_t N> void use_remote_models(const RemoteModelSeed* (&in_remote_model_seeds)[N])
		{
			use_remote_models(in_remote_model_seeds, N);
		}
		void use_remote_models(const RemoteModelSeed** in_remote_model_seeds, size_t num_remote_model_seeds);

		void set_root_workload(const WorkloadSeed& root_workload, bool auto_finalize_and_validate = true);

		void set_telemetry_port(const uint16_t telemetry_port);

		// general-purpose finalise function (bakes and validates as needed):
		void finalize();

		// accessors:
		const char* get_model_name() const { return model_name.empty() ? "model_name_not_set" : model_name.c_str(); };

		const ArrayView<const WorkloadSeed*>& get_workload_seeds() const { return workload_seeds; }
		const ArrayView<const DataConnectionSeed*>& get_data_connection_seeds() const { return data_connection_seeds; }
		const ArrayView<const RemoteModelSeed*>& get_remote_models() const { return remote_models; }

		const WorkloadSeed* get_root_workload() const { return root_workload; }
		uint16_t get_telemetry_port() const { return telemetry_port; };

	  protected:
#ifdef ROBOTICK_ENABLE_MODEL_HEAP
		void bake_dynamic_workloads();
		void bake_dynamic_data_connections();
		void bake_dynamic_remote_models();
#endif

	  private:
		StringView model_name;

		ArrayView<const WorkloadSeed*> workload_seeds;
		ArrayView<const DataConnectionSeed*> data_connection_seeds;
		ArrayView<const RemoteModelSeed*> remote_models;

		const WorkloadSeed* root_workload = nullptr;

		uint16_t telemetry_port = 7090;

#ifdef ROBOTICK_ENABLE_MODEL_HEAP
		List<WorkloadSeed> workload_seeds_storage;
		List<DataConnectionSeed> data_connection_seeds_storage;
		List<RemoteModelSeed> remote_models_storage;

		HeapVector<const WorkloadSeed*> baked_workload_ptrs;
		HeapVector<const DataConnectionSeed*> baked_data_connection_ptrs;
		HeapVector<const RemoteModelSeed*> baked_remote_model_ptrs;
#endif
	};

} // namespace robotick
