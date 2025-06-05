// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/Engine.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/utils/WorkloadFieldsIterator.h"
#include "robotick/platform/MqttClient.h"

#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace robotick
{
	class MqttFieldSync
	{
	  public:
		// For unit tests: only a publisher lambda
		using PublisherFn = std::function<void(const std::string&, const std::string&, bool)>;

		/** Constructor for tests (no Engine/IMqttClient) */
		MqttFieldSync(const std::string& root_ns, PublisherFn publisher);

		/** Constructor for real use: links to Engine and an existing IMqttClient */
		MqttFieldSync(Engine& engine, const std::string& root_ns, IMqttClient& mqtt_client);

		/** Subscribe to "<root>/control/#" and publish initial fields (state+control) */
		void subscribe_and_sync_startup();

		/** Apply any queued control updates into the Engine’s main buffer */
		void apply_control_updates();

		/** Publish only state fields (no control) to "<root>/state/…" */
		void publish_state_fields();

		/**
		 * Publish all fields under "<root>/state/…" and optionally "<root>/control/…"
		 *   - engine:    the Engine whose fields we iterate
		 *   - buffer:    the snapshot buffer (WorkloadsBuffer) to read from
		 *   - publish_control: if true, also emit to "<root>/control/…"
		 */
		void publish_fields(const Engine& engine, const WorkloadsBuffer& buffer, bool publish_control);

	  private:
		std::string root;	   // e.g. "robotick"
		PublisherFn publisher; // for unit tests only
		IMqttClient* mqtt_ptr; // null in test-mode
		Engine* engine_ptr = nullptr;
		std::unordered_map<std::string, nlohmann::json> last_published; // topic → last JSON value sent
		std::unordered_map<std::string, nlohmann::json> updated_topics; // control updates received

		/** Serialize a single field (by pointer and TypeId) into JSON */
		nlohmann::json serialize(void* ptr, TypeId type);
	};
} // namespace robotick
