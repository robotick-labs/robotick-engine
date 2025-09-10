// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/RemoteEngineConnections.h"
#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/model/Model.h"

namespace robotick
{

	void RemoteEngineConnections::setup(Engine& in_engine, const Model& model)
	{
		engine = &in_engine;
		const char* my_model_name = model.get_model_name();
		ROBOTICK_ASSERT(my_model_name != nullptr);

		discoverer_receiver.initialize_receiver(my_model_name);
		discoverer_receiver.set_on_incoming_connection_requested(
			[this, &model](const char* source_model_id, int& rec_port_out)
			{
				RemoteEngineConnection& conn = dynamic_receivers.push_back();
				conn.configure_receiver(model.get_model_name());

				conn.set_field_binder(
					[this](const char* path, RemoteEngineConnection::Field& out)
					{
						auto [ptr, size, field_desc] = DataConnectionUtils::find_field_info(*engine, path);
						if (!ptr)
						{
							ROBOTICK_FATAL_EXIT("Receiver failed to bind field: %s", path);
						}
						out.path = path;
						out.recv_ptr = ptr;
						out.size = size;
						out.type_desc = field_desc->find_type_descriptor();
						return true;
					});

				// Tick to get assigned port
				for (int i = 0; i < 10; ++i)
				{
					conn.tick(TICK_INFO_FIRST_10MS_100HZ);
					if (conn.get_listen_port() > 0)
						break;
				}
				rec_port_out = conn.get_listen_port();
				ROBOTICK_INFO("[DISCOVERY] Responding to '%s' with port %d", source_model_id, rec_port_out);
			});

		const auto& remote_model_seeds = model.get_remote_models();
		if (remote_model_seeds.empty())
			return;

		senders.initialize(remote_model_seeds.size());
		discoverer_sender.initialize_sender(my_model_name, "*"); // Wildcard target

		discoverer_sender.set_on_remote_model_discovered(
			[this, &model](const RemoteEngineDiscoverer::PeerInfo& peer)
			{
				for (auto& pending : pending_senders)
				{
					if (pending.remote_model_name == peer.model_id)
					{
						if (!pending.connection->has_basic_connection())
						{
							pending.connection->configure_sender(model.get_model_name(), peer.model_id.c_str(), peer.ip.c_str(), peer.port);
						}
					}
				}
			});

		uint32_t index = 0;
		for (const auto* remote_model : remote_model_seeds)
		{
			if (remote_model->remote_data_connection_seeds.empty())
				continue;

			RemoteEngineConnection& conn = senders[index++];

			PendingSender& p = pending_senders.push_back();
			p.remote_model_name = remote_model->model_name;
			p.connection = &conn;

			for (const auto* conn_seed : remote_model->remote_data_connection_seeds)
			{
				auto [ptr, size, field_desc] = DataConnectionUtils::find_field_info(*engine, conn_seed->source_field_path.c_str());
				if (!ptr)
				{
					ROBOTICK_FATAL_EXIT("Failed to resolve source: %s", conn_seed->source_field_path.c_str());
				}

				RemoteEngineConnection::Field f;
				f.path = conn_seed->dest_field_path.c_str();
				f.send_ptr = ptr;
				f.size = size;
				f.type_desc = field_desc->find_type_descriptor();
				ROBOTICK_ASSERT(f.type_desc);

				conn.register_field(f);
			}
		}
	}

	void RemoteEngineConnections::tick(const TickInfo& tick_info)
	{
		discoverer_receiver.tick(tick_info);
		discoverer_sender.tick(tick_info);

		for (auto& r : dynamic_receivers)
			r.tick(tick_info);

		for (auto& s : senders)
			s.tick(tick_info);
	}

} // namespace robotick