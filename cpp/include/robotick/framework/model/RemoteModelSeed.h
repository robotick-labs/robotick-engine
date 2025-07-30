// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"
#include "robotick/framework/common/ArrayView.h"
#include "robotick/framework/common/List.h"
#include "robotick/framework/common/StringView.h"
#include "robotick/framework/model/DataConnectionSeed.h"

#ifdef ROBOTICK_ENABLE_MODEL_HEAP
#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/common/HeapVector.h"
#endif

namespace robotick
{
	struct WorkloadSeed;

	struct RemoteModelSeed
	{
		friend class Model;

		enum class Mode
		{
			IP,
			UART,
			Local
		};

		RemoteModelSeed() = default;

		RemoteModelSeed(
			const StringView& model_name, const Mode comms_mode, const StringView& comms_channel, const ArrayView<const DataConnectionSeed*>& seeds)
			: model_name(model_name)
			, comms_mode(comms_mode)
			, comms_channel(comms_channel)
			, remote_data_connection_seeds(seeds)
		{
		}

		StringView model_name;

		Mode comms_mode = Mode::Local;
		StringView comms_channel; // e.g. "/dev/ttyUSB0", "192.168.1.42", etc.

		ArrayView<const DataConnectionSeed*> remote_data_connection_seeds;

#ifdef ROBOTICK_ENABLE_MODEL_HEAP
		void set_model_name(const char* in_model_name)
		{
			model_name_storage = in_model_name;
			model_name = model_name_storage.c_str();
		}

		void set_comms_channel(const char* in_channel)
		{
			comms_channel_storage = in_channel;
			comms_channel = comms_channel_storage.c_str();
		}

		RemoteModelSeed& connect(const char* source_field_path_local, const char* dest_field_path_remote)
		{
			for (const auto& s : remote_data_connection_seeds_storage)
			{
				if (s.dest_field_path == dest_field_path_remote)
					ROBOTICK_FATAL_EXIT("Remote destination field in model '%s' already has an incoming remote-connection: %s",
						model_name.c_str(),
						dest_field_path_remote);
			}

			DataConnectionSeed& data_connection_seed = remote_data_connection_seeds_storage.push_back();
			data_connection_seed.set_source_field_path(source_field_path_local);
			data_connection_seed.set_dest_field_path(dest_field_path_remote);

			return *this;
		}

	  protected:
		void bake_dynamic_remote_connections()
		{
			baked_remote_data_connections.initialize(remote_data_connection_seeds_storage.size());

			size_t index = 0;
			for (auto& seed : remote_data_connection_seeds_storage)
			{
				baked_remote_data_connections[index] = &seed;
				++index;
			}

			remote_data_connection_seeds.use(baked_remote_data_connections.data(), baked_remote_data_connections.size());
		}

	  protected:
		List<DataConnectionSeed> remote_data_connection_seeds_storage;
		FixedString64 model_name_storage;
		FixedString64 comms_channel_storage;

		HeapVector<const DataConnectionSeed*> baked_remote_data_connections;
#endif
	};

} // namespace robotick
