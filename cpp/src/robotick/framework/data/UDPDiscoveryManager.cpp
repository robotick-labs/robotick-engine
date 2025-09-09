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

	void UDPDiscoveryManager::initialize(const char* my_model_name, int service_port, DiscoveryMode mode)
	{
		this->mode = mode;
		this->my_model_id = my_model_name;
		this->listen_port = service_port;

		if (mode == DiscoveryMode::Receiver)
		{
			init_recv_socket(); // receive multicast discovery
			init_send_socket(); // send unicast reply
		}
		else if (mode == DiscoveryMode::Sender)
		{
			init_send_socket(); // send multicast discovery
			init_recv_socket(); // receive unicast reply
		}
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

			if (setsockopt(recv_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
			{
				ROBOTICK_WARNING("[%s] Failed to join multicast group", my_model_id.c_str());
				close(recv_fd);
				recv_fd = -1;
				return;
			}
		}
		else
		{
			// Store dynamically assigned reply port (for DISCOVER_PEER)
			sockaddr_in bound{};
			socklen_t len = sizeof(bound);
			if (getsockname(recv_fd, (sockaddr*)&bound, &len) == 0)
			{
				sender_reply_port = ntohs(bound.sin_port);
				ROBOTICK_INFO("[%s] Sender recv socket bound to ephemeral port %d", my_model_id.c_str(), sender_reply_port);
			}
		}

		int flags = fcntl(recv_fd, F_GETFL, 0);
		fcntl(recv_fd, F_SETFL, flags | O_NONBLOCK);
	}

	void UDPDiscoveryManager::init_send_socket()
	{
		send_fd = socket(AF_INET, SOCK_DGRAM, 0);
		if (send_fd < 0)
		{
			ROBOTICK_WARNING("[%s] Failed to create send socket", my_model_id.c_str());
			return;
		}

		unsigned char ttl = 1;
		setsockopt(send_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

		unsigned char loop = 1;
		setsockopt(send_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

		ROBOTICK_INFO("[%s] Send socket initialized", my_model_id.c_str());
	}

	void UDPDiscoveryManager::set_on_discovered(OnDiscoveredCallback cb)
	{
		callback = std::move(cb);
	}

	void UDPDiscoveryManager::broadcast_discovery_request(const char* target_model_name)
	{
		if (mode != DiscoveryMode::Sender || send_fd < 0)
			return;

		char buffer[256];
		int len = snprintf(buffer, sizeof(buffer), "%s %s %d", DISCOVER_MSG, target_model_name, sender_reply_port);

		sockaddr_in target{};
		target.sin_family = AF_INET;
		target.sin_port = htons(DISCOVERY_PORT);
		target.sin_addr.s_addr = inet_addr(MULTICAST_GROUP);

		ROBOTICK_INFO("[%s] Sending DISCOVER_PEER -> '%s'", my_model_id.c_str(), buffer);
		sendto(send_fd, buffer, len, 0, (sockaddr*)&target, sizeof(target));
	}

	void UDPDiscoveryManager::tick()
	{
		char buffer[256];
		sockaddr_in sender{};
		socklen_t addr_len = sizeof(sender);

		int fd = recv_fd;
		if (fd < 0)
			return;

		int len = recvfrom(fd, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&sender, &addr_len);
		if (len <= 0)
			return;

		buffer[len] = '\0';
		handle_incoming_packet(buffer, sender);
	}

	void UDPDiscoveryManager::handle_incoming_packet(const char* data, const sockaddr_in& sender)
	{
		char sender_ip[INET_ADDRSTRLEN] = {};
		inet_ntop(AF_INET, &sender.sin_addr, sender_ip, sizeof(sender_ip));

		ROBOTICK_INFO("[%s] Received: '%s' from %s:%d", my_model_id.c_str(), data, sender_ip, ntohs(sender.sin_port));

		if (strncmp(data, DISCOVER_MSG, strlen(DISCOVER_MSG)) == 0 && mode == DiscoveryMode::Receiver)
		{
			char target_id[64] = {};
			int reply_port = -1;
			if (sscanf(data, "DISCOVER_PEER %63s %d", target_id, &reply_port) != 2)
			{
				ROBOTICK_WARNING("[%s] Malformed discovery request: '%s'", my_model_id.c_str(), data);
				return;
			}
			if (!my_model_id.equals(target_id))
				return;

			char reply[256];
			int reply_len = snprintf(reply, sizeof(reply), "%s %s %d", PEER_REPLY_MSG, my_model_id.c_str(), listen_port);

			sockaddr_in dest{};
			dest.sin_family = AF_INET;
			dest.sin_port = htons(reply_port);
			inet_pton(AF_INET, sender_ip, &dest.sin_addr);

			ROBOTICK_INFO("[%s] Sending PEER_HERE reply to %s:%d", my_model_id.c_str(), sender_ip, reply_port);
			sendto(send_fd, reply, reply_len, 0, (sockaddr*)&dest, sizeof(dest));
		}
		else if (strncmp(data, PEER_REPLY_MSG, strlen(PEER_REPLY_MSG)) == 0 && mode == DiscoveryMode::Sender)
		{
			char model_id[64] = {};
			int port = -1;
			if (sscanf(data, "PEER_HERE %63s %d", model_id, &port) != 2 || port <= 0)
			{
				ROBOTICK_WARNING("[%s] Malformed or invalid PEER_REPLY_MSG: '%s'", my_model_id.c_str(), data);
				return;
			}

			if (callback)
			{
				PeerInfo info;
				info.model_id = model_id;
				info.ip = inet_ntoa(sender.sin_addr);
				info.port = port;
				callback(info);
			}
		}
	}

} // namespace robotick