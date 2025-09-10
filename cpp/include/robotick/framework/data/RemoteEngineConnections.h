// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api.h"
#include "robotick/framework/common/HeapVector.h"
#include "robotick/framework/common/List.h"
#include "robotick/framework/data/RemoteEngineConnection.h"
#include "robotick/framework/data/RemoteEngineDiscoverer.h"

namespace robotick
{
	class Engine;
	class Model;
	struct TickInfo;

	class RemoteEngineConnections
	{
	  public:
		void setup(Engine& engine, const Model& model);
		void tick(const TickInfo& tick_info);

	  private:
		Engine* engine = nullptr;

		HeapVector<RemoteEngineDiscoverer> discoverer_senders;
		HeapVector<RemoteEngineConnection> senders;

		RemoteEngineDiscoverer discoverer_receiver;
		List<RemoteEngineConnection> dynamic_receivers;
	};
} // namespace robotick
