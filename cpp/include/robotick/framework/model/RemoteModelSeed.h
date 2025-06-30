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
	class Model;
	struct WorkloadSeed;

	struct RemoteModelSeed
	{
		friend class Model;

		StringView model_name = nullptr;

		enum class Mode
		{
			IP,
			UART,
			Local
		} comms_mode = Mode::Local;

		StringView comms_channel = nullptr; // e.g. "/dev/ttyUSB0", "192.168.1.42", etc.
		const Model* model = nullptr;

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

		void bake_dynamic_remote_connections()
		{
			baked_remote_data_connections.initialize(remote_data_connection_seeds_storage.size());

			size_t index = 0;
			for (auto& seed : remote_data_connection_seeds_storage)
			{
				baked_remote_data_connections[index] = &seed;
			}

			remote_data_connection_seeds.use(baked_remote_data_connections);
		}

	  protected:
		List<DataConnectionSeed> remote_data_connection_seeds_storage;
		FixedString64 model_name_storage;
		FixedString64 comms_channel_storage;

		HeapVector<const DataConnectionSeed*> baked_remote_data_connections;
#endif
	};

} // namespace robotick
