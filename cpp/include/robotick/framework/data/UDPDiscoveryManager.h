// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/FixedString.h"
#include <functional>
#include <netinet/in.h> // Needed for sockaddr_in

namespace robotick
{
	enum class DiscoveryMode
	{
		Sender,	 // Only sends multicast discovery requests, and listens for unicast replies
		Receiver // Listens for multicast discovery messages, and replies via unicast
	};

	class UDPDiscoveryManager
	{
	  public:
		struct PeerInfo
		{
			FixedString64 model_id;
			FixedString64 ip; // e.g. "192.168.1.42"
			int port;
		};

		using OnDiscoveredCallback = std::function<void(const PeerInfo&)>;

		UDPDiscoveryManager();
		~UDPDiscoveryManager();

		void initialize(const char* my_model_name, int service_port, DiscoveryMode mode); // setup sockets based on mode
		void broadcast_discovery_request(const char* target_model_name);				  // emits DISCOVER_PEER <target> <reply_port>
		void set_on_discovered(OnDiscoveredCallback cb);								  // called with PEER_HERE response

		void tick(); // poll socket(s)

	  protected:
		void init_send_socket();
		void init_recv_socket();
		void handle_incoming_packet(const char* data, const sockaddr_in& sender);

	  private:
		DiscoveryMode mode = DiscoveryMode::Sender;

		int recv_fd = -1;
		int send_fd = -1;
		int reply_fd = -1; // Sender uses this to receive unicast replies
		int sender_reply_port = 0;
		int listen_port = 0; // Service port used in replies from receiver

		FixedString64 my_model_id;
		OnDiscoveredCallback callback = nullptr;
	};
} // namespace robotick