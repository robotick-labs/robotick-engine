#include "robotick/framework/data/TelemetryServer.h"

#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/utils/WorkloadFieldsIterator.h"

namespace robotick
{
	void TelemetryServer::start(const Engine& engine_in, const uint16_t telemetry_port)
	{
		engine = &engine_in;

		web_server.start("Telemetry",
			telemetry_port,
			nullptr,
			[this](const WebRequest& req, WebResponse& res)
			{
				if (!req.method.equals("GET"))
				{
					res.status_code = 404;
					return true;
				}

				if (!engine)
				{
					res.body.set_from_string("{\"error\":\"no engine available\"}");
					res.status_code = 500;
					return true;
				}

				if (req.uri.equals("/"))
				{
					handle_get_home_page(req, res);
					return true;
				}
				else if (req.uri.equals("/api/telemetry/workloads"))
				{
					handle_get_workloads(req, res);
					return true;
				}
				else if (req.uri.equals("/api/telemetry/workload/stats"))
				{
					handle_get_workload_stats(req, res);
					return true;
				}
				else if (req.uri.equals("/api/telemetry/workload/config"))
				{
					handle_get_workload_config(req, res);
					return true;
				}
				else if (req.uri.equals("/api/telemetry/workload/inputs"))
				{
					handle_get_workload_inputs(req, res);
					return true;
				}
				else if (req.uri.equals("/api/telemetry/workload/outputs"))
				{
					handle_get_workload_outputs(req, res);
					return true;
				}
				else if (req.uri.equals("/api/telemetry/workload/output_png"))
				{
					handle_get_workload_output_png(req, res);
					return true;
				}

				return false;
			});
	}

	void TelemetryServer::stop()
	{
		web_server.stop();
	}

	void TelemetryServer::handle_get_home_page(const WebRequest& /*unused*/, WebResponse& res)
	{
		constexpr const char telemetry_html[] = R"(
			<!DOCTYPE html>
			<html>
			<head>
			<meta charset="utf-8">
			<title>Robotick | Telemetry</title>
			<style>
				body { font-family: sans-serif; padding: 1em; background: #f0f0f0; }
				table { border-collapse: collapse; width: 100%; background: white; }
				th, td { border: 1px solid #ccc; padding: 4px 8px; text-align: left; vertical-align: top; }
				th { background: #eee; }
				pre { margin: 0; white-space: pre-wrap; word-break: break-word; }
				div.multiline {
					white-space: pre-wrap;
					word-break: break-word;
					overflow-wrap: anywhere;
				}
			</style>
			</head>
			<body>
			<h1>Robotick | Telemetry</h1>
			<table id="telemetry">
				<thead>
				<tr>
					<th>Unique Name</th>
					<th>Type</th>
					<th>Config</th>
					<th>Inputs</th>
					<th>Outputs</th>
					<th>Self Duration (ms)</th>
					<th>Time Delta (ms)</th>
					<th>Goal Period (ms)</th>
					<th>Usage %</th>
				</tr>
				</thead>
				<tbody></tbody>
			</table>
			<script>
				const workloads = [];
				let workloadIndex = 0;

				async function fetchJSON(url) {
					try {
						const res = await fetch(url);
						return await res.json();
					} catch (err) {
						console.warn("Failed to fetch:", url, err);
						return null;
					}
				}

				async function fetchWorkloadDetails(name) {
					const [config, inputs, outputs] = await Promise.all([
						fetchJSON(`/api/telemetry/workload/config?name=${name}`),
						fetchJSON(`/api/telemetry/workload/inputs?name=${name}`),
						fetchJSON(`/api/telemetry/workload/outputs?name=${name}`),
					]);

					const wl = workloads.find(w => w.name === name);
					if (!wl) return;
					wl.config = config;
					wl.inputs = inputs;
					wl.outputs = outputs;
				}

				async function fetchWorkloadLiveData(name) {
					const stats = await fetchJSON(`/api/telemetry/workload/stats?name=${name}`);
					const inputs = await fetchJSON(`/api/telemetry/workload/inputs?name=${name}`);
					const outputs = await fetchJSON(`/api/telemetry/workload/outputs?name=${name}`);

					const wl = workloads.find(w => w.name === name);
					if (!wl) return;

					if (stats) {
						wl.self_ms = stats.self_ms;
						wl.dt_ms = stats.dt_ms;
						wl.goal_ms = stats.goal_ms;
					}
					if (inputs) wl.inputs = inputs;
					if (outputs) wl.outputs = outputs;

					renderTelemetryTable();
				}

				function formatKeyValue(obj) {
					if (!obj || typeof obj !== "object") return "–";
					return Object.entries(obj).map(([k, v]) => `${k}: ${v}`).join("\n");
				}

				function renderTelemetryTable() {
					const tbody = document.querySelector("#telemetry tbody");
					tbody.innerHTML = "";

					for (const w of workloads) {
						const row = document.createElement("tr");

						const raw_self = typeof w.self_ms === "number" ? w.self_ms : null;
						const raw_goal = typeof w.goal_ms === "number" ? w.goal_ms : null;

						const self_ms = typeof w.self_ms === "number" ? w.self_ms.toFixed(1) : "–";
						const dt_ms = typeof w.dt_ms === "number" ? w.dt_ms.toFixed(1) : "–";
						const goal_ms = typeof w.goal_ms === "number" ? w.goal_ms.toFixed(1) : "–";

						const load_pct = (raw_self !== null && raw_goal > 0)
							? ((raw_self / raw_goal) * 100).toFixed(1)
							: "–";

						const config = formatKeyValue(w.config);
						const inputs = formatKeyValue(w.inputs);
						const outputs = formatKeyValue(w.outputs);

						row.innerHTML = `
							<td>${w.name}</td>
							<td>${w.type}</td>
							<td><div class="multiline">${config}</div></td>
							<td><div class="multiline">${inputs}</div></td>
							<td><div class="multiline">${outputs}</div></td>
							<td>${self_ms}</td>
							<td>${dt_ms}</td>
							<td>${goal_ms}</td>
							<td>${load_pct}</td>
						`;
						tbody.appendChild(row);
					}
				}

				async function loadInitialData() {
					const data = await fetchJSON('/api/telemetry/workloads');
					if (!data || !Array.isArray(data.workloads)) return;

					for (const w of data.workloads) {
						workloads.push({
							name: w.name ?? "–",
							type: w.type ?? "–",
							dt_ms: null,
							goal_ms: null,
							load_pct: null,
							config: null,
							inputs: null,
							outputs: null
						});
						fetchWorkloadDetails(w.name);
					}
				}

				// Periodically fetch live stats + I/O
				setInterval(() => {
					if (workloads.length === 0) return;
					const w = workloads[workloadIndex % workloads.length];
					workloadIndex++;
					fetchWorkloadLiveData(w.name);
				}, 100);

				loadInitialData();
			</script>
			</body>
			</html>
			)";

		res.status_code = 200;
		res.content_type = "text/html";
		res.body.set_from_string(telemetry_html);
	}

	void TelemetryServer::handle_get_workloads(const WebRequest& /*req*/, WebResponse& res)
	{
		const auto& instances = engine->get_all_instance_info();
		res.body.clear();
		res.body.append_from_string("{\"workloads\":[");

		bool first = true;
		for (const auto& info : instances)
		{
			if (!first)
				res.body.append_from_string(",");
			first = false;
			res.body.append_from_string_format("{\"name\":\"%s\",\"type\":\"%s\"}", info.seed->unique_name.c_str(), info.type->name.c_str());
		}

		res.body.append_from_string("]}");
		res.status_code = 200;
		res.content_type = "application/json";
	}

	void TelemetryServer::handle_get_workload_stats(const WebRequest& req, WebResponse& res)
	{
		const char* workload_unique_name = req.find_query_param("name");
		const WorkloadInstanceInfo* info = workload_unique_name != nullptr ? engine->find_instance_info(workload_unique_name) : nullptr;
		if (!info)
		{
			res.body.set_from_string("{\"error\":\"not found\"}");
			res.status_code = 404;
			return;
		}

		res.body.clear();
		res.body.append_from_string_format("{\"self_ms\":%.2f,\"dt_ms\":%.2f,\"goal_ms\":%.1f}",
			info->mutable_stats.get_last_tick_duration_ms(),
			info->mutable_stats.get_last_time_delta_ms(),
			(info->seed->tick_rate_hz > 0.0) ? (1000.0f / info->seed->tick_rate_hz) : -1.0f);

		res.status_code = 200;
		res.content_type = "application/json";
	}

	static void emit_fields_as_json(WebResponseBodyBuffer& out,
		const WorkloadInstanceInfo& info,
		const TypeDescriptor* struct_desc,
		size_t struct_offset,
		WorkloadsBuffer& buffer)
	{
		out.append_from_string("{");

		bool first = true;
		WorkloadFieldsIterator::for_each_field_in_struct(info,
			struct_desc,
			struct_offset,
			buffer,
			[&](const WorkloadFieldView& view)
			{
				if (!first)
					out.append_from_string(",");
				first = false;

				const char* subfield_separator = "";
				const char* subfield_name = "";
				if (view.subfield_info)
				{
					subfield_separator = ".";
					subfield_name = view.subfield_info->name.c_str();
				}

				const TypeDescriptor* td = view.subfield_info ? view.subfield_info->find_type_descriptor() : view.field_info->find_type_descriptor();

				FixedString256 value = "\"<invalid>\"";
				if (td && view.field_ptr && buffer.contains_object(view.field_ptr, td->size))
				{
					if (!td->to_string(view.field_ptr, value.data, value.capacity()))
					{
						value.format("<%s>", td->name);
					}
				}

				out.append_from_string_format("\"%s%s%s\":\"%s\"", view.field_info->name.c_str(), subfield_separator, subfield_name, value.c_str());
			});

		out.append_from_string("}");
	}

	void TelemetryServer::handle_get_workload_config(const WebRequest& req, WebResponse& res)
	{
		const char* name = req.find_query_param("name");
		const WorkloadInstanceInfo* info = name ? engine->find_instance_info(name) : nullptr;

		if (!info)
		{
			res.body.set_from_string("{\"error\":\"not found\"}");
			res.status_code = 404;
			return;
		}

		WorkloadsBuffer& mirror = engine->get_workloads_buffer();
		res.body.clear();

		emit_fields_as_json(res.body, *info, info->type->get_workload_desc()->config_desc, info->type->get_workload_desc()->config_offset, mirror);

		res.status_code = 200;
		res.content_type = "application/json";
	}

	void TelemetryServer::handle_get_workload_io(const WebRequest& req, WebResponse& res, bool inputs)
	{
		const char* name = req.find_query_param("name");
		const WorkloadInstanceInfo* info = name ? engine->find_instance_info(name) : nullptr;

		if (!info)
		{
			res.body.set_from_string("{\"error\":\"not found\"}");
			res.status_code = 404;
			return;
		}

		const WorkloadDescriptor* desc = info->type->get_workload_desc();
		const TypeDescriptor* struct_desc = inputs ? desc->inputs_desc : desc->outputs_desc;
		size_t struct_offset = inputs ? desc->inputs_offset : desc->outputs_offset;

		WorkloadsBuffer& mirror = engine->get_workloads_buffer();
		res.body.clear();

		emit_fields_as_json(res.body, *info, struct_desc, struct_offset, mirror);

		res.status_code = 200;
		res.content_type = "application/json";
	}

	void TelemetryServer::handle_get_workload_inputs(const WebRequest& req, WebResponse& res)
	{
		handle_get_workload_io(req, res, true);
	}

	void TelemetryServer::handle_get_workload_outputs(const WebRequest& req, WebResponse& res)
	{
		handle_get_workload_io(req, res, false);
	}

	void TelemetryServer::handle_get_workload_output_png(const WebRequest& req, WebResponse& res)
	{
		const char* workload_unique_name = req.find_query_param("name");
		const char* field_id = req.find_query_param("field");

		if (!workload_unique_name || !field_id)
		{
			res.body.set_from_string("{\"error\":\"missing query params: require name and field\"}");
			res.status_code = 400;
			res.content_type = "application/json";
			return;
		}

		// Construct full path: e.g., workload.outputs.my.field
		FixedString256 full_path;
		full_path.format("%s.outputs.%s", workload_unique_name, field_id);

		auto [ptr, size, field_desc] = DataConnectionUtils::find_field_info(*engine, full_path.c_str());
		if (!ptr || !field_desc)
		{
			res.body.set_from_string("{\"error\":\"field not found in outputs\"}");
			res.status_code = 404;
			res.content_type = "application/json";
			return;
		}

		const uint8_t* obj = static_cast<const uint8_t*>(ptr);
		const size_t obj_size = size;

		auto is_png_sig = [](const uint8_t* p) -> bool
		{
			static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
			for (int i = 0; i < 8; i++)
				if (p[i] != sig[i])
					return false;
			return true;
		};

		auto be32 = [](const uint8_t* p) -> uint32_t
		{
			return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
		};

		// Locate PNG start
		size_t png_start = SIZE_MAX;
		for (size_t i = 0; i + 8 <= obj_size; ++i)
		{
			if (is_png_sig(obj + i))
			{
				png_start = i;
				break;
			}
		}

		if (png_start == SIZE_MAX)
		{
			res.body.set_from_string("{\"error\":\"no PNG signature found in field data\"}");
			res.status_code = 422;
			res.content_type = "application/json";
			return;
		}

		// Find IEND chunk
		size_t cur = png_start + 8;
		while (true)
		{
			if (cur + 12 > obj_size)
			{
				res.body.set_from_string("{\"error\":\"truncated PNG before IEND\"}");
				res.status_code = 422;
				res.content_type = "application/json";
				return;
			}

			uint32_t chunk_len = be32(obj + cur);

			// Prevent integer overflow when computing next chunk position
			if (chunk_len > obj_size - cur - 12)
			{
				res.body.set_from_string("{\"error\":\"invalid PNG chunk length\"}");
				res.status_code = 422;
				res.content_type = "application/json";
				return;
			}

			const uint8_t* type = obj + cur + 4;
			size_t next = cur + 4 + 4 + chunk_len + 4;

			if (next > obj_size)
			{
				res.body.set_from_string("{\"error\":\"invalid PNG chunk length\"}");
				res.status_code = 422;
				res.content_type = "application/json";
				return;
			}

			if (type[0] == 'I' && type[1] == 'E' && type[2] == 'N' && type[3] == 'D')
			{
				size_t png_size = next - png_start;
				res.body.set(obj + png_start, png_size);
				res.status_code = 200;
				res.content_type = "image/png";
				return;
			}

			cur = next;
		}
	}

} // namespace robotick
