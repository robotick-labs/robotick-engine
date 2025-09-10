// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api.h"
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

		RemoteEngineDiscoverer discoverer_receiver;
		RemoteEngineDiscoverer discoverer_sender;

		List<RemoteEngineConnection> dynamic_receivers;
		HeapVector<RemoteEngineConnection> senders;

		struct PendingSender
		{
			FixedString64 remote_model_name;
			RemoteEngineConnection* connection = nullptr;
		};

		List<PendingSender> pending_senders;
	};
} // namespace robotick
