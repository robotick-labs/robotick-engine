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
		WorkloadSeed& add(const char* type, const char* name);
		void connect(const char* source_field_path, const char* dest_field_path);
		void add_remote_model(const Model& remote_model, const char* model_name, const char* comms_channel);

#endif
		void set_workloads(const WorkloadSeed** all_workloads, size_t num_workloads);

		void set_root_workload(const WorkloadSeed& root_workload, bool auto_finalize_and_validate = true);

		const ArrayView<const WorkloadSeed*>& get_workload_seeds() const { return workload_seeds; }

		const ArrayView<const DataConnectionSeed*>& get_data_connection_seeds() const { return data_connection_seeds; }

		const ArrayView<const RemoteModelSeed*>& get_remote_models() const { return remote_models; }

		const WorkloadSeed* get_root_workload() const { return root_workload; }

		void finalize();

	  protected:
#ifdef ROBOTICK_ENABLE_MODEL_HEAP
		void connect_remote(const char* source_field_path, const char* dest_field_path);

		void bake_dynamic_workloads();
		void bake_dynamic_data_connections();
		void bake_dynamic_remote_models();
#endif

	  private:
		ArrayView<const WorkloadSeed*> workload_seeds;
		ArrayView<const DataConnectionSeed*> data_connection_seeds;
		ArrayView<const RemoteModelSeed*> remote_models;

		const WorkloadSeed* root_workload = nullptr;

#ifdef ROBOTICK_ENABLE_MODEL_HEAP
		List<WorkloadSeed> workload_seeds_storage;
		List<DataConnectionSeed> data_connection_seeds_storage;
		List<RemoteModelSeed> remote_models_storage;
		List<FixedString64> strings_storage;

		HeapVector<const WorkloadSeed*> baked_workload_ptrs;
		HeapVector<const DataConnectionSeed*> baked_data_connection_ptrs;
		HeapVector<const RemoteModelSeed*> baked_remote_model_ptrs;
#endif
	};

} // namespace robotick
