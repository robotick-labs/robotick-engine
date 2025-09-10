// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/UDPDiscoveryManager.h"
#include "robotick/api.h"

#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace robotick
{
	constexpr const char* MULTICAST_GROUP = "239.10.77.42";
	constexpr int DISCOVERY_PORT = 49777;
	constexpr float BROADCAST_INTERVAL_SEC = 0.1f; // 10Hz

	constexpr const char* DISCOVER_MSG = "DISCOVER_PEER";
	constexpr const char* PEER_REPLY_MSG = "PEER_HERE";

	UDPDiscoveryManager::UDPDiscoveryManager()
	{
	}

	UDPDiscoveryManager::~UDPDiscoveryManager()
	{
		if (recv_fd >= 0)
			close(recv_fd);
		if (send_fd >= 0)
			close(send_fd);
	}

	void UDPDiscoveryManager::initialize_sender(const char* my_model_name, const char* target_model_name)
	{
		mode = DiscoveryMode::Sender;
		my_model_id = my_model_name;
		target_model_id = target_model_name;
		status = DiscoveryStatus::ReadyToBroadcast;
		time_sec_to_broadcast = 0.0f;
		init_send_socket();
		init_recv_socket();
	}

	void UDPDiscoveryManager::initialize_receiver(const char* my_model_name)
	{
		mode = DiscoveryMode::Receiver;
		my_model_id = my_model_name;
		init_recv_socket();
		init_send_socket();
	}

	void UDPDiscoveryManager::reset_discovery()
	{
		if (mode == DiscoveryMode::Sender)
		{
			status = DiscoveryStatus::ReadyToBroadcast;
			time_sec_to_broadcast = 0.0f;
		}
	}

	void UDPDiscoveryManager::set_on_remote_model_discovered(OnRemoteModelDiscovered cb)
	{
		on_discovered_cb = std::move(cb);
	}

	void UDPDiscoveryManager::set_on_incoming_connection_requested(OnIncomingConnectionRequested cb)
	{
		on_requested_cb = std::move(cb);
	}

	void UDPDiscoveryManager::init_recv_socket()
	{
		recv_fd = socket(AF_INET, SOCK_DGRAM, 0);
		if (recv_fd < 0)
		{
			ROBOTICK_WARNING("[%s] Failed to create recv socket", my_model_id.c_str());
			return;
		}

		int reuse = 1;
		setsockopt(recv_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

		sockaddr_in local_addr{};
		local_addr.sin_family = AF_INET;

		if (mode == DiscoveryMode::Receiver)
		{
			local_addr.sin_port = htons(DISCOVERY_PORT);
			local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		}
		else // Sender listens on ephemeral port for reply
		{
			local_addr.sin_port = htons(0);
			local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		}

		if (bind(recv_fd, (sockaddr*)&local_addr, sizeof(local_addr)) < 0)
		{
			ROBOTICK_WARNING("[%s] Failed to bind recv socket", my_model_id.c_str());
			close(recv_fd);
			recv_fd = -1;
			return;
		}

		if (mode == DiscoveryMode::Receiver)
		{
			ip_mreq mreq{};
			mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);
			mreq.imr_interface.s_addr = htonl(INADDR_ANY);
			setsockopt(recv_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
		}
		else
		{
			sockaddr_in bound{};
			socklen_t len = sizeof(bound);
			if (getsockname(recv_fd, (sockaddr*)&bound, &len) == 0)
				sender_reply_port = ntohs(bound.sin_port);
		}

		int flags = fcntl(recv_fd, F_GETFL, 0);
		fcntl(recv_fd, F_SETFL, flags | O_NONBLOCK);
	}

	void UDPDiscoveryManager::init_send_socket()
	{
		send_fd = socket(AF_INET, SOCK_DGRAM, 0);
		if (send_fd < 0)
			return;

		unsigned char ttl = 1;
		socklen_t ttl_len = sizeof(ttl);
		setsockopt(send_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, ttl_len);

		unsigned char loop = 1;
		socklen_t loop_len = sizeof(loop);
		setsockopt(send_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, loop_len);
	}

	void UDPDiscoveryManager::broadcast_discovery_request(const char* target_model_name)
	{
		if (send_fd < 0)
			return;

		char buffer[256];
		int len = snprintf(buffer, sizeof(buffer), "%s %s %d", DISCOVER_MSG, target_model_name, sender_reply_port);

		sockaddr_in target{};
		target.sin_family = AF_INET;
		target.sin_port = htons(DISCOVERY_PORT);
		target.sin_addr.s_addr = inet_addr(MULTICAST_GROUP);

		sendto(send_fd, buffer, len, 0, (sockaddr*)&target, sizeof(target));
		status = DiscoveryStatus::WaitingForReply;
	}

	void UDPDiscoveryManager::tick(const TickInfo& tick_info)
	{
		// Always poll for incoming packets
		char buffer[256];
		sockaddr_in sender{};
		socklen_t addr_len = sizeof(sender);

		if (recv_fd >= 0)
		{
			int len = recvfrom(recv_fd, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&sender, &addr_len);
			if (len > 0)
			{
				buffer[len] = '\0';
				handle_incoming_packet(buffer, sender);
			}
		}

		// Sender-mode periodic broadcast
		if (mode == DiscoveryMode::Sender)
		{
			if (time_sec_to_broadcast > 0.0f)
			{
				time_sec_to_broadcast -= static_cast<float>(tick_info.delta_time);
				return;
			}

			broadcast_discovery_request(target_model_id.c_str());

			// Reset countdown
			time_sec_to_broadcast = BROADCAST_INTERVAL_SEC;
		}
	}

	void UDPDiscoveryManager::handle_incoming_packet(const char* data, const sockaddr_in& sender)
	{
		char sender_ip[INET_ADDRSTRLEN] = {};
		inet_ntop(AF_INET, &sender.sin_addr, sender_ip, sizeof(sender_ip));

		if (strncmp(data, DISCOVER_MSG, strlen(DISCOVER_MSG)) == 0 && mode == DiscoveryMode::Receiver)
		{
			char target_id[64] = {};
			char source_id[64] = {};
			int reply_port = -1;
			if (sscanf(data, "%*s %63s %63s %d", target_id, source_id, &reply_port) != 3)
				return;

			if (!my_model_id.equals(target_id))
				return;

			int dynamic_rec_port = listen_port;
			if (on_requested_cb)
				on_requested_cb(source_id, dynamic_rec_port);

			char reply[256];
			int reply_len = snprintf(reply, sizeof(reply), "%s %s %d", PEER_REPLY_MSG, my_model_id.c_str(), dynamic_rec_port);

			sockaddr_in dest{};
			dest.sin_family = AF_INET;
			dest.sin_port = htons(reply_port);
			inet_pton(AF_INET, sender_ip, &dest.sin_addr);

			sendto(send_fd, reply, reply_len, 0, (sockaddr*)&dest, sizeof(dest));
		}
		else if (strncmp(data, PEER_REPLY_MSG, strlen(PEER_REPLY_MSG)) == 0 && mode == DiscoveryMode::Sender)
		{
			char model_id[64] = {};
			int port = -1;
			if (sscanf(data, "%*s %63s %d", model_id, &port) != 2)
				return;

			PeerInfo info;
			info.model_id = model_id;
			info.ip = inet_ntoa(sender.sin_addr);
			info.port = port;

			if (on_discovered_cb)
				on_discovered_cb(info);

			status = DiscoveryStatus::Discovered;
		}
	}

} // namespace robotick
