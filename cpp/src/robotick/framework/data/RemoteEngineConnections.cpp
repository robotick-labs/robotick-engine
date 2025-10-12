// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/RemoteEngineConnections.h"
#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/model/Model.h"

#define ROBOTICK_REMOTE_ENGINE_CONNECTIONS_VERBOSE 0

namespace robotick
{

	void RemoteEngineConnections::setup(Engine& in_engine, const Model& model)
	{
		const bool log_verbose = ROBOTICK_REMOTE_ENGINE_CONNECTIONS_VERBOSE == 1;

		engine = &in_engine;
		const char* my_model_name = model.get_model_name();
		ROBOTICK_ASSERT(my_model_name != nullptr);

		ROBOTICK_INFO_IF(log_verbose, "[REC::setup] Setting up RemoteEngineConnections for model '%s'", my_model_name);

		discoverer_receiver.initialize_receiver(my_model_name);
		discoverer_receiver.set_on_incoming_connection_requested(
			[this, &model](const char* source_model_id, uint16_t& rec_port_out)
			{
				ROBOTICK_INFO_IF(log_verbose, "[REC::receiver] Incoming discovery request from model '%s'", source_model_id);
				RemoteEngineConnection& conn = dynamic_receivers.push_back();
				conn.configure_receiver(model.get_model_name());

				conn.set_field_binder(
					[this](const char* path, RemoteEngineConnection::Field& out)
					{
						ROBOTICK_INFO_IF(log_verbose, "[REC::receiver] Binding field '%s'", path);

						auto [ptr, size, field_desc] = DataConnectionUtils::find_field_info(*engine, path);
						if (!ptr)
						{
							ROBOTICK_FATAL_EXIT("[REC::receiver] Receiver failed to bind field: %s", path);
						}
						out.path = path;
						out.recv_ptr = ptr;
						out.size = size;
						out.type_desc = field_desc->find_type_descriptor();
						return true;
					});

				for (int i = 0; i < 10; ++i)
				{
					conn.tick(TICK_INFO_FIRST_10MS_100HZ);
					if (conn.get_listen_port() > 0)
						break;
				}
				rec_port_out = conn.get_listen_port();

				ROBOTICK_INFO_IF(log_verbose, "[DISCOVERY] Responding to '%s' with port %d", source_model_id, rec_port_out);
			});

		const auto& remote_model_seeds = model.get_remote_models();
		if (remote_model_seeds.empty())
		{
			ROBOTICK_INFO_IF(log_verbose, "[REC::setup - %s] No remote models declared; skipping sender setup", my_model_name);
			return;
		}

		ROBOTICK_INFO_IF(log_verbose, "[REC::setup] Declared %d remote model(s)", (int)remote_model_seeds.size());

		senders.initialize(remote_model_seeds.size());
		discoverer_senders.initialize(remote_model_seeds.size());

		uint32_t index = 0;
		for (const auto* remote_model : remote_model_seeds)
		{
			ROBOTICK_INFO_IF(log_verbose, "[REC::setup] Remote model seed: '%s'", remote_model->model_name.c_str());

			if (remote_model->remote_data_connection_seeds.empty())
			{
				ROBOTICK_INFO_IF(log_verbose, "[REC::setup] Model '%s' has no connections; skipping", remote_model->model_name.c_str());
				continue;
			}

			RemoteEngineConnection& remote_connection = senders[index];
			RemoteEngineDiscoverer& discoverer_sender = discoverer_senders[index];
			index++;

			discoverer_sender.initialize_sender(my_model_name, remote_model->model_name.c_str());
			discoverer_sender.set_on_remote_model_discovered(
				[&](const RemoteEngineDiscoverer::PeerInfo& peer)
				{
					ROBOTICK_INFO("[REC::sender] Discovered remote model '%s' at %s:%d", peer.model_id.c_str(), peer.ip.c_str(), peer.port);
					if (!remote_connection.has_basic_connection())
					{
						ROBOTICK_INFO_IF(log_verbose, "[REC::sender] Configuring sender to model '%s'", peer.model_id.c_str());
						remote_connection.configure_sender(model.get_model_name(), peer.model_id.c_str(), peer.ip.c_str(), peer.port);
					}
				});

			for (const auto* conn_seed : remote_model->remote_data_connection_seeds)
			{
				const char* src = conn_seed->source_field_path.c_str();
				const char* dst = conn_seed->dest_field_path.c_str();

				ROBOTICK_INFO_IF(log_verbose, "[REC::setup] Binding sender field '%s' → '%s'", src, dst);

				auto [ptr, size, field_desc] = DataConnectionUtils::find_field_info(*engine, src);
				if (!ptr)
				{
					ROBOTICK_FATAL_EXIT("[REC::setup] Failed to resolve sender source: %s", src);
				}

				RemoteEngineConnection::Field f;
				f.path = dst;
				f.send_ptr = ptr;
				f.size = size;
				f.type_desc = field_desc->find_type_descriptor();
				ROBOTICK_ASSERT(f.type_desc);

				remote_connection.register_field(f);
			}
		}

		ROBOTICK_INFO_IF(log_verbose, "[REC::setup] Finished setup");
	}

	void RemoteEngineConnections::tick(const TickInfo& tick_info)
	{
		discoverer_receiver.tick(tick_info);

		for (size_t i = 0; i < discoverer_senders.size(); ++i)
		{
			RemoteEngineConnection& conn = senders[i];
			RemoteEngineDiscoverer& disc = discoverer_senders[i];

			if (!conn.has_basic_connection())
			{
				// only tick discoverer-senders for which associated remote-connection hasn't even bound a socket
				disc.tick(tick_info);
			}
		}

		for (auto& r : dynamic_receivers)
			r.tick(tick_info);

		for (auto& s : senders)
			s.tick(tick_info);
	}

} // namespace robotick