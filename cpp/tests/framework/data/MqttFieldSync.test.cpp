
// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#if 0

#include "robotick/framework/data/MqttFieldSync.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
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

			TestInputs()
				: blackboard({BlackboardFieldInfo("flag", TypeId(GET_TYPE_ID(int))), BlackboardFieldInfo("ratio", TypeId(GET_TYPE_ID(double)))})
			{
				*reinterpret_cast<int*>(blackboard.get_field_info("flag")->get_data_ptr(blackboard)) = 1;
				*reinterpret_cast<double*>(blackboard.get_field_info("ratio")->get_data_ptr(blackboard)) = 0.5;
			}
		};
		ROBOTICK_BEGIN_FIELDS(TestInputs)
		ROBOTICK_FIELD(TestInputs, int, value)
		ROBOTICK_FIELD(TestInputs, FixedString64, text)
		ROBOTICK_FIELD(TestInputs, Blackboard, blackboard)
		ROBOTICK_END_FIELDS()

		struct TestWorkload
		{
			TestInputs inputs;
		};
		ROBOTICK_DEFINE_WORKLOAD(TestWorkload, void, TestInputs, void)

		struct DummyMqttClient : public IMqttClient
		{
			std::unordered_map<std::string, std::string> retained;

			void connect() override {}
			void subscribe(const std::string&) override {}
			void publish(const std::string& topic, const std::string& payload, bool retain) override
			{
				if (retain)
					retained[topic] = payload;
			}
			void set_callback(std::function<void(std::string, std::string)>) override {}
		};
	} // namespace

	TEST_CASE("Publish state and control fields")
	{
		Model model;
		auto h = model.add("TestWorkload", "W1", 1.0);
		model.set_root(h);

		Engine engine;
		engine.load(model);

		const auto& info = EngineInspector::get_all_instance_info(engine)[0];
		auto* wptr = static_cast<TestWorkload*>((void*)info.get_ptr(engine));
		wptr->inputs.value = 42;
		*reinterpret_cast<int*>(wptr->inputs.blackboard.get_field_info("flag")->get_data_ptr(wptr->inputs.blackboard)) = 2;
		*reinterpret_cast<double*>(wptr->inputs.blackboard.get_field_info("ratio")->get_data_ptr(wptr->inputs.blackboard)) = 3.14;

		WorkloadsBuffer mirror_buf(engine.get_workloads_buffer().get_size());
		mirror_buf.update_mirror_from(engine.get_workloads_buffer());

		DummyMqttClient dummy;
		std::string root_ns = "robotick";
		MqttFieldSync sync(engine, root_ns, dummy);

		sync.subscribe_and_sync_startup();
		// Check retained messages contain both state and control for inputs
		CHECK(dummy.retained.count("robotick/state/W1/inputs/value"));
		CHECK(dummy.retained.count("robotick/state/W1/inputs/text"));
		CHECK(dummy.retained.count("robotick/state/W1/inputs/blackboard/flag"));
		CHECK(dummy.retained.count("robotick/state/W1/inputs/blackboard/ratio"));

		CHECK(dummy.retained.count("robotick/control/W1/inputs/value"));
		CHECK(dummy.retained.count("robotick/control/W1/inputs/text"));
		CHECK(dummy.retained.count("robotick/control/W1/inputs/blackboard/flag"));
		CHECK(dummy.retained.count("robotick/control/W1/inputs/blackboard/ratio"));

		// Clear retained and test publish_state_fields only
		dummy.retained.clear();
		sync.publish_state_fields();
		CHECK(dummy.retained.count("robotick/state/W1/inputs/value"));
		CHECK_FALSE(dummy.retained.count("robotick/control/W1/inputs/value"));
	}

	TEST_CASE("Apply control updates")
	{
		Model model;
		auto h = model.add("TestWorkload", "W2", 1.0);
		model.set_root(h);

		Engine engine;
		engine.load(model);

		DummyMqttClient dummy;
		std::string root_ns = "robotick";
		MqttFieldSync sync(engine, root_ns, dummy);

		// Simulate receiving a control message for value and subfield
		nlohmann::json json_val = 99;
		nlohmann::json json_subint = 5;
		sync.updated_topics["robotick/control/W2/inputs/value"] = json_val;
		sync.updated_topics["robotick/control/W2/inputs/blackboard/flag"] = json_subint;

		sync.apply_control_updates();

		const auto& info = EngineInspector::get_all_instance_info(engine)[0];
		auto* wptr = static_cast<TestWorkload*>((void*)info.get_ptr(engine));
		int val = wptr->inputs.value;
		int flag = *reinterpret_cast<int*>(wptr->inputs.blackboard.get_field_info("flag")->get_data_ptr(wptr->inputs.blackboard));

		CHECK(val == 99);
		CHECK(flag == 5);
	}

} // namespace robotick::test

#endif // #if 0
