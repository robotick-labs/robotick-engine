// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"
#include "robotick/framework/common/ArrayView.h"
#include "robotick/framework/common/List.h"
#include "robotick/framework/common/StringView.h"
#include "robotick/framework/model_v2/DataConnectionSeed.h"

#ifdef ROBOTICK_ENABLE_MODEL_HEAP
#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/common/HeapVector.h"
#endif

namespace robotick
{
	class Model_v2;

	struct RemoteModelSeed_v2
	{
		friend class Model_v2;

		StringView model_name = nullptr;

		enum class Mode
		{
			IP,
			UART,
			Local
		} comms_mode = Mode::Local;

		StringView comms_channel = nullptr; // e.g. "/dev/ttyUSB0", "192.168.1.42", etc.
		const Model_v2* model = nullptr;

		ArrayView<DataConnectionSeed_v2*> remote_data_connection_seeds;

#ifdef ROBOTICK_ENABLE_MODEL_HEAP
		RemoteModelSeed_v2& set_model_name(const char* in_model_name);
		RemoteModelSeed_v2& set_comms_channel(const char* in_channel);

	  protected:
		List<DataConnectionSeed_v2> remote_data_connection_seeds_storage;
		FixedString64 model_name_storage;
		FixedString64 comms_channel_storage;
#endif
	};

} // namespace robotick
