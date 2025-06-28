// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"
#include "robotick/framework/common/ArrayView.h"
#include "robotick/framework/common/StringView.h"
#include "robotick/framework/model_v2/DataConnectionSeed.h"
#include "robotick/framework/model_v2/RemoteModelSeed.h"
#include "robotick/framework/model_v2/WorkloadSeed.h"

#ifdef ROBOTICK_ENABLE_MODEL_HEAP
#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/common/HeapVector.h"
#include "robotick/framework/common/List.h"
#endif

#include <cstdint>

namespace robotick
{
	class Model_v2
	{
	  public:
		static constexpr float TICK_RATE_FROM_PARENT = 0.0f;

#ifdef ROBOTICK_ENABLE_MODEL_HEAP
		WorkloadSeed_v2& add(const char* type, const char* name);
		void connect(const char* source_field_path, const char* dest_field_path);
		void add_remote_model(const Model_v2& remote_model, const char* model_name, const char* comms_channel);

#endif
		void set_workloads(const WorkloadSeed_v2** all_workloads, size_t num_workloads);

		void set_root_workload(const WorkloadSeed_v2& root_workload, bool auto_finalize = true);

		const ArrayView<const WorkloadSeed_v2*>& get_workload_seeds() const
		{
			return workload_seeds;
		}

		const ArrayView<const DataConnectionSeed_v2*>& get_data_connection_seeds() const
		{
			return data_connection_seeds;
		}

		const ArrayView<const RemoteModelSeed_v2*>& get_remote_models() const
		{
			return remote_models;
		}

		const WorkloadSeed_v2* get_root_workload() const
		{
			return root_workload;
		}

		void finalize();

	  protected:
#ifdef ROBOTICK_ENABLE_MODEL_HEAP
		void connect_remote(const char* source_field_path, const char* dest_field_path);

		void bake_dynamic_workloads();
		void bake_dynamic_data_connections();
		void bake_dynamic_remote_models();
#endif

	  private:
		ArrayView<const WorkloadSeed_v2*> workload_seeds;
		ArrayView<const DataConnectionSeed_v2*> data_connection_seeds;
		ArrayView<const RemoteModelSeed_v2*> remote_models;

		const WorkloadSeed_v2* root_workload = nullptr;

#ifdef ROBOTICK_ENABLE_MODEL_HEAP
		List<WorkloadSeed_v2> workload_seeds_storage;
		List<DataConnectionSeed_v2> data_connection_seeds_storage;
		List<RemoteModelSeed_v2> remote_models_storage;

		HeapVector<const WorkloadSeed_v2*> baked_workload_ptrs;
		HeapVector<const DataConnectionSeed_v2*> baked_data_connection_ptrs;
		HeapVector<const RemoteModelSeed_v2*> baked_remote_model_ptrs;
#endif
	};

} // namespace robotick
