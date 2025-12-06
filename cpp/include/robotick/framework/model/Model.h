// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"
#include "robotick/framework/containers/ArrayView.h"
#include "robotick/framework/model/DataConnectionSeed.h"
#include "robotick/framework/model/RemoteModelSeed.h"
#include "robotick/framework/model/WorkloadSeed.h"
#include "robotick/framework/strings/StringView.h"

#include <cstdint>

namespace robotick
{
	class Model
	{
	  public:
		// non-dynamic modifiers:
		void set_model_name(const char* in_model_name) { model_name = in_model_name; }

		template <size_t N> void use_workload_seeds(const WorkloadSeed* const (&in_seeds)[N]) { use_workload_seeds(in_seeds, N); }
		void use_workload_seeds(const WorkloadSeed* const* all_workloads, size_t num_workloads);

		template <size_t N> void use_data_connection_seeds(const DataConnectionSeed* const (&in_connections)[N])
		{
			use_data_connection_seeds(in_connections, N);
		}
		void use_data_connection_seeds(const DataConnectionSeed* const* in_connections, size_t num_connections);

		template <size_t N> void use_remote_models(const RemoteModelSeed* const (&in_remote_model_seeds)[N])
		{
			use_remote_models(in_remote_model_seeds, N);
		}
		void use_remote_models(const RemoteModelSeed* const* in_remote_model_seeds, size_t num_remote_model_seeds);

		void set_root_workload(const WorkloadSeed& root_workload, bool auto_finalize_and_validate = true);

		void set_telemetry_port(const uint16_t in_telemetry_port);

		// general-purpose finalise function (bakes and validates as needed):
		void finalize();

		// accessors:
		const char* get_model_name() const { return model_name.empty() ? "model_name_not_set" : model_name.c_str(); };

		const ArrayView<const WorkloadSeed*>& get_workload_seeds() const { return workload_seeds; }
		const ArrayView<const DataConnectionSeed*>& get_data_connection_seeds() const { return data_connection_seeds; }
		const ArrayView<const RemoteModelSeed*>& get_remote_models() const { return remote_models; }

		const WorkloadSeed* get_root_workload() const { return root_workload; }
		uint16_t get_telemetry_port() const { return telemetry_port; };

	  private:
		StringView model_name;

		ArrayView<const WorkloadSeed*> workload_seeds;
		ArrayView<const DataConnectionSeed*> data_connection_seeds;
		ArrayView<const RemoteModelSeed*> remote_models;

		const WorkloadSeed* root_workload = nullptr;

		uint16_t telemetry_port = 7090;
	};

} // namespace robotick
