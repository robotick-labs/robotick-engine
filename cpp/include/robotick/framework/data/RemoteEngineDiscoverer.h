// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/FixedString.h"
#include <functional>

struct sockaddr_in;

namespace robotick
{
	struct TickInfo;

	class RemoteEngineDiscoverer
	{
	  public:
		struct PeerInfo
		{
			FixedString64 model_id;
			FixedString64 ip; // e.g. "192.168.1.42"
			uint16_t port;
		};

		// Callback fired on sender when a receiver responds to our discovery broadcast
		using OnRemoteModelDiscovered = std::function<void(const PeerInfo&)>;

		// Callback fired on receiver when a discovery request arrives from a remote model
		// Should populate the dynamic receiver port to use when replying
		using OnIncomingConnectionRequested = std::function<void(const char* source_model_id, uint16_t& rec_port_id)>;

		RemoteEngineDiscoverer();
		~RemoteEngineDiscoverer();

		void initialize_sender(const char* my_model_name, const char* target_model_name);
		void initialize_receiver(const char* my_model_name);

		// Register callbacks for sender/receiver mode
		void set_on_incoming_connection_requested(OnIncomingConnectionRequested cb);
		void set_on_remote_model_discovered(OnRemoteModelDiscovered cb);

		// Tick function should be called every frame or periodically
		void tick(const TickInfo& tick_info);

		// Reset state and resume periodic broadcasts
		void reset_discovery();

	  protected:
		void init_send_socket();
		void init_recv_socket();
		void broadcast_discovery_request(const char* target_model_name);
		void handle_incoming_packet(const char* data, const sockaddr_in& sender);

	  private:
		enum class DiscoveryMode
		{
			Sender,	 // Broadcasts multicast discovery, listens for replies
			Receiver // Listens for multicast discovery, replies via unicast
		};

		enum class DiscoveryStatus
		{
			ReadyToBroadcast,
			WaitingForReply,
			Discovered
		};

		DiscoveryMode mode = DiscoveryMode::Sender;
		DiscoveryStatus status = DiscoveryStatus::ReadyToBroadcast;

		int recv_fd = -1;
		int send_fd = -1;
		int reply_fd = -1; // Sender uses this to receive unicast replies
		uint16_t sender_reply_port = 0;
		uint16_t listen_port = 0; // Service port used in replies from receiver

		FixedString64 my_model_id;
		FixedString64 target_model_id;

		OnRemoteModelDiscovered on_discovered_cb = nullptr;
		OnIncomingConnectionRequested on_requested_cb = nullptr;

		float time_sec_to_broadcast = 0.0f;
	};
} // namespace robotick