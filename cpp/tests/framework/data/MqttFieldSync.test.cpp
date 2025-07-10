// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/MqttFieldSync.h"
#include "../utils/BlackboardTestUtils.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/utils/TypeId.h"
#include "robotick/framework/utils/WorkloadFieldsIterator.h"

#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace robotick::test
{

	namespace
	{
		struct TestInputs
		{
			int value = 7;
			FixedString64 text = "abc";
			Blackboard blackboard;
		};
		ROBOTICK_REGISTER_STRUCT_BEGIN(TestInputs)
		ROBOTICK_STRUCT_FIELD(TestInputs, int, value)
		ROBOTICK_STRUCT_FIELD(TestInputs, FixedString64, text)
		ROBOTICK_STRUCT_FIELD(TestInputs, Blackboard, blackboard)
		ROBOTICK_REGISTER_STRUCT_END(TestInputs)

		struct TestState
		{
			HeapVector<FieldDescriptor> fields;
		};

		struct TestWorkload
		{
			TestInputs inputs;
			State<TestState> state;

			void pre_load()
			{
				state->fields.initialize(2);
				state->fields[0] = FieldDescriptor{"flag", GET_TYPE_ID(int)};
				state->fields[1] = FieldDescriptor{"ratio", GET_TYPE_ID(double)};
				inputs.blackboard.initialize_fields(state->fields);
			}

			void load()
			{
				inputs.blackboard.set("flag", 1);
				inputs.blackboard.set("ratio", 0.5);
			}
		};
		ROBOTICK_REGISTER_WORKLOAD(TestWorkload, void, TestInputs, void)

		struct DummyMqttClient : public IMqttClient
		{
			std::unordered_map<std::string, std::string> retained;

			void connect() override {}
			void subscribe(const std::string& /*topic*/, int /*qos*/ = 1) override {}

			void publish(const std::string& topic, const std::string& payload, bool retain = true) override
			{
				if (retain)
					retained[topic] = payload;
			}

			void set_callback(std::function<void(const std::string&, const std::string&)>) override {}
		};
	} // namespace

	TEST_CASE("Unit/Framework/Data/MqttFieldSync")
	{
		SECTION("MqttFieldSync can publish state and control fields")
		{
			Model model;
			const WorkloadSeed& test_workload_seed = model.add("TestWorkload", "W1").set_tick_rate_hz(1.0f);
			model.set_root_workload(test_workload_seed);

			Engine engine;
			engine.load(model);

			// initialize our input fields & blackboard-fields:
			const auto& info = *engine.find_instance_info(test_workload_seed.unique_name);
			auto* test_workload_ptr = static_cast<TestWorkload*>((void*)info.get_ptr(engine));
			test_workload_ptr->inputs.value = 42;
			test_workload_ptr->inputs.blackboard.set("flag", 2);
			test_workload_ptr->inputs.blackboard.set("ratio", 3.14);

			WorkloadsBuffer mirror_buf;
			mirror_buf.create_mirror_from(engine.get_workloads_buffer());

			DummyMqttClient dummy_client;
			std::string root_topic_name = "robotick";
			MqttFieldSync sync(engine, root_topic_name, dummy_client);

			sync.subscribe_and_sync_startup();

			// Check retained messages contain both state and control for inputs
			CHECK(dummy_client.retained.count("robotick/state/W1/inputs/value"));
			CHECK(dummy_client.retained.count("robotick/state/W1/inputs/text"));
			CHECK(dummy_client.retained.count("robotick/state/W1/inputs/blackboard/flag"));
			CHECK(dummy_client.retained.count("robotick/state/W1/inputs/blackboard/ratio"));

			CHECK(dummy_client.retained.count("robotick/control/W1/inputs/value"));
			CHECK(dummy_client.retained.count("robotick/control/W1/inputs/text"));
			CHECK(dummy_client.retained.count("robotick/control/W1/inputs/blackboard/flag"));
			CHECK(dummy_client.retained.count("robotick/control/W1/inputs/blackboard/ratio"));

			// Clear retained and test publish_state_fields only
			dummy_client.retained.clear();
			sync.publish_state_fields();
			CHECK(dummy_client.retained.count("robotick/state/W1/inputs/value"));
			CHECK_FALSE(dummy_client.retained.count("robotick/control/W1/inputs/value"));
		}

		SECTION("MqttFieldSync can apply control updates")
		{
			Model model;
			const WorkloadSeed& test_workload_seed = model.add("TestWorkload", "W2").set_tick_rate_hz(1.0f);
			model.set_root_workload(test_workload_seed);

			Engine engine;
			engine.load(model);

			const auto& info = *engine.find_instance_info(test_workload_seed.unique_name);
			auto* test_workload_ptr = static_cast<TestWorkload*>((void*)info.get_ptr(engine));

			DummyMqttClient dummy_client;
			std::string root_topic_name = "robotick";
			MqttFieldSync sync(engine, root_topic_name, dummy_client);

			nlohmann::json json_val = 99;
			nlohmann::json json_subint = 5;
			sync.get_updated_topics()["robotick/control/W2/inputs/value"] = json_val;
			sync.get_updated_topics()["robotick/control/W2/inputs/blackboard/flag"] = json_subint;

			sync.apply_control_updates();

			int val = test_workload_ptr->inputs.value;
			int flag = test_workload_ptr->inputs.blackboard.get<int>("flag");

			CHECK(val == 99);
			CHECK(flag == 5);
		}
	}

} // namespace robotick::test
