// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"
#include "robotick/framework/data/DataConnection.h"

#include <algorithm>
#include <any>
#include <cassert>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace robotick
{
	struct RemoteModelSeed_v1;

	struct WorkloadHandle_v1
	{
		uint32_t index = uint32_t(-1); // Indicates invalid/unset by default
		bool is_valid() const { return index != uint32_t(-1); }
	};

	struct WorkloadSeed_v1
	{
		std::string type;
		std::string name;
		double tick_rate_hz;
		std::vector<WorkloadHandle_v1> children;
		std::map<std::string, std::string> config;
	};

	class Model_v1
	{
	  public:
		static constexpr double TICK_RATE_FROM_PARENT = 0.0;

		WorkloadHandle_v1 add(const std::string& type, const std::string& name, double tick_rate_hz = TICK_RATE_FROM_PARENT,
			const std::map<std::string, std::string>& config = {});

		WorkloadHandle_v1 add(const std::string& type, const std::string& name, const std::vector<WorkloadHandle_v1>& children,
			double tick_rate_hz = TICK_RATE_FROM_PARENT, const std::map<std::string, std::string>& config = {});

		void connect(const std::string& source_field_path, const std::string& dest_field_path);

		void add_remote_model(const Model_v1& remote_model, const std::string& model_name, const std::string& comms_channel);

		void set_root(WorkloadHandle_v1 handle, const bool auto_finalize = true);

		const std::vector<WorkloadSeed_v1>& get_workload_seeds() const { return workload_seeds; }

		const std::vector<DataConnectionSeed_v1>& get_data_connection_seeds() const { return data_connection_seeds; }

		const std::unordered_map<std::string, RemoteModelSeed_v1>& get_remote_models() const { return remote_models; }

		WorkloadHandle_v1 get_root() const { return root_workload; }

		void finalize();

	  protected:
		void connect_remote(const std::string& source_field_path, const std::string& dest_field_path);

	  private:
		std::vector<WorkloadSeed_v1> workload_seeds;
		std::vector<DataConnectionSeed_v1> data_connection_seeds;

		std::unordered_map<std::string, RemoteModelSeed_v1> remote_models;

		WorkloadHandle_v1 root_workload; // <- Root workload entry point
	};

	struct RemoteModelSeed_v1
	{
		std::string model_name;

		enum class Mode
		{
			IP,
			UART,
			Local
		} comms_mode;

		std::string comms_channel; // e.g. "/dev/ttyUSB0", "192.168.1.42", ""
		Model_v1 model;

		std::vector<DataConnectionSeed_v1> remote_data_connection_seeds;
	};

} // namespace robotick
