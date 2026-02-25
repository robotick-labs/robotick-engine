// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"
#include "robotick/framework/containers/ArrayView.h"
#include "robotick/framework/containers/List.h"
#include "robotick/framework/model/DataConnectionSeed.h"
#include "robotick/framework/strings/StringView.h"

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

		RemoteModelSeed(const StringView& model_name, const ArrayView<const DataConnectionSeed*>& seeds)
			: model_name(model_name)
			, remote_data_connection_seeds(seeds)
		{
		}

		StringView model_name;

		Mode comms_mode = Mode::Local;
		StringView comms_channel; // e.g. "/dev/ttyUSB0", "192.168.1.42", etc.

		ArrayView<const DataConnectionSeed*> remote_data_connection_seeds;
	};

} // namespace robotick
