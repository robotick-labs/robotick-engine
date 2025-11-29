// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api.h"
#include "robotick/framework/containers/List.h"
#include "robotick/framework/data/RemoteEngineConnection.h"
#include "robotick/framework/data/RemoteEngineDiscoverer.h"
#include "robotick/framework/memory/HeapVector.h"

namespace robotick
{
	class Engine;
	class Model;
	struct TickInfo;

	class RemoteEngineConnections
	{
	  public:
		RemoteEngineConnections() = default;
		RemoteEngineConnections(const RemoteEngineConnections&) = delete;
		RemoteEngineConnections(RemoteEngineConnections&&) noexcept = default;

		~RemoteEngineConnections();

		RemoteEngineConnections& operator=(const RemoteEngineConnections&) = delete;
		RemoteEngineConnections& operator=(RemoteEngineConnections&&) noexcept = default;

		void setup(Engine& engine, const Model& model);
		void stop();
		void tick(const TickInfo& tick_info);

	  private:
		Engine* engine = nullptr;

		// Invariant: discoverer_senders[i] corresponds to senders[i]
		HeapVector<RemoteEngineDiscoverer> discoverer_senders;
		HeapVector<RemoteEngineConnection> senders;

		RemoteEngineDiscoverer discoverer_receiver;
		List<RemoteEngineConnection> dynamic_receivers;
	};
} // namespace robotick
