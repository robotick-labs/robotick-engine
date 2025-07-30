#include "robotick/framework/data/TelemetryServer.h"

#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/utils/WorkloadFieldsIterator.h"

namespace robotick
{
	constexpr uint16_t TELEMETRY_SERVER_PORT = 7090;

	void TelemetryServer::start(const Engine& engine_in)
	{
		engine = &engine_in;

		web_server.start("Telemetry",
			TELEMETRY_SERVER_PORT,
			nullptr,
			[this](const WebRequest& req, WebResponse& res)
			{
				if (!req.method.equals("GET"))
				{
					res.status_code = 404;
					return;
				}

				if (req.uri.equals("/telemetry/workloads"))
				{
					handle_get_workloads(req, res);
					return;
				}

				// Not handled
				res.body.set_from_string("Unknown endpoint");
				res.status_code = 404;
			});
	}

	void TelemetryServer::stop()
	{
		web_server.stop();
	}

	void TelemetryServer::handle_get_workloads(const WebRequest& /*unused*/, WebResponse& res)
	{
		if (!engine)
		{
			res.body.set_from_string("{\"error\":\"no engine available\"}");
			res.status_code = 500;
			res.content_type = "application/json";
			return;
		}

		WorkloadsBuffer& mirror_buffer = engine->get_workloads_buffer(); // TODO - consider making it into a mirror, but not on-demand

		const auto& instances = engine->get_all_instance_info();
		res.body.clear();
		res.body.append_from_string("{\"workloads\":[");

		bool first = true;
		for (const robotick::WorkloadInstanceInfo& info : instances)
		{
			if (!first)
				res.body.append_from_string(",");
			first = false;

			// Type & name
			res.body.append_from_string_format("{\"name\":\"%s\",\"type\":\"%s\",", info.seed->unique_name.c_str(), info.type->name.c_str());

			// Config fields

			res.body.append_from_string("\"config\":{");
			bool first_config = true;
			WorkloadFieldsIterator::for_each_field_in_struct(info,
				info.type->get_workload_desc()->config_desc,
				info.type->get_workload_desc()->config_offset,
				mirror_buffer,
				[&](const WorkloadFieldView& view)
				{
					if (!first_config)
						res.body.append_from_string(",");
					first_config = false;

					const TypeDescriptor* field_type_desc =
						view.subfield_info ? view.subfield_info->find_type_descriptor() : view.field_info->find_type_descriptor();

					void* field_ptr = view.field_ptr;

					FixedString256 value;
					if (field_type_desc && field_ptr && mirror_buffer.contains_object(field_ptr, field_type_desc->size))
					{
						if (!field_type_desc->to_string(field_ptr, value.data, value.capacity()))
						{
							value.format("\"<%s>\"", field_type_desc->name);
						}
					}
					else
					{
						value = "\"<invalid>\"";
					}

					res.body.append_from_string_format("\"%s\":%s", view.field_info->name.c_str(), value.c_str()); // value should be valid JSON
				});

			res.body.append_from_string("},");

			// Inputs
			res.body.append_from_string("\"inputs\":[");
			bool first_input = true;
			WorkloadFieldsIterator::for_each_field_in_struct(info,
				info.type->get_workload_desc()->inputs_desc,
				info.type->get_workload_desc()->inputs_offset,
				mirror_buffer,
				[&](const WorkloadFieldView& view)
				{
					if (!first_input)
						res.body.append_from_string(",");
					first_input = false;
					res.body.append_from_string_format("\"%s\"", view.field_info->name.c_str());
				});
			res.body.append_from_string("],");

			// Outputs
			res.body.append_from_string("\"outputs\":[");
			bool first_output = true;
			WorkloadFieldsIterator::for_each_field_in_struct(info,
				info.type->get_workload_desc()->outputs_desc,
				info.type->get_workload_desc()->outputs_offset,
				mirror_buffer,
				[&](const WorkloadFieldView& view)
				{
					if (!first_output)
						res.body.append_from_string(",");
					first_output = false;
					res.body.append_from_string_format("\"%s\"", view.field_info->name.c_str());
				});
			res.body.append_from_string("]}");
		}

		res.body.append_from_string("]}");
		res.content_type = "application/json";
		res.status_code = 200;
	}

} // namespace robotick
