#include "HostDiscovery.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <fstream>
#include <sstream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace moonlight::network {
namespace {

struct PendingConnection {
  int fd;
  std::string address;
};

std::set<std::string> localSubnetAddresses() {
  ifaddrs* rawInterfaces = nullptr;
  std::set<std::string> result;
  std::set<std::uint32_t> networks;
  if (getifaddrs(&rawInterfaces) == 0) {
    for (const ifaddrs* interface = rawInterfaces; interface != nullptr;
         interface = interface->ifa_next) {
      if (interface->ifa_addr == nullptr ||
          interface->ifa_addr->sa_family != AF_INET ||
          (interface->ifa_flags & IFF_UP) == 0 ||
          (interface->ifa_flags & IFF_LOOPBACK) != 0) {
        continue;
      }

      const auto* address =
          reinterpret_cast<const sockaddr_in*>(interface->ifa_addr);
      const std::uint32_t local = ntohl(address->sin_addr.s_addr);
      networks.insert(local & 0xffffff00U);
    }
    freeifaddrs(rawInterfaces);
  }

  // A connected UDP socket lets the kernel select the active LAN interface
  // without sending a packet. This works in restricted app namespaces where
  // interface enumeration and /proc are filtered.
  const int routeSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (routeSocket >= 0) {
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &destination.sin_addr);
    if (connect(
            routeSocket,
            reinterpret_cast<const sockaddr*>(&destination),
            sizeof(destination)) == 0) {
      sockaddr_in localEndpoint{};
      socklen_t localEndpointLength = sizeof(localEndpoint);
      if (getsockname(
              routeSocket,
              reinterpret_cast<sockaddr*>(&localEndpoint),
              &localEndpointLength) == 0) {
        const std::uint32_t local = ntohl(localEndpoint.sin_addr.s_addr);
        networks.clear();
        networks.insert(local & 0xffffff00U);
      }
    }
    close(routeSocket);
  }

  // Some Vega application sandboxes omit interface addresses from
  // getifaddrs(). The kernel routing table still exposes directly connected
  // IPv4 networks, so use it as a discovery fallback.
  if (networks.empty()) {
    std::ifstream routes("/proc/net/route");
    std::string line;
    std::getline(routes, line);
    while (std::getline(routes, line)) {
      std::istringstream fields(line);
      std::string interfaceName;
      std::string destinationText;
      std::string gatewayText;
      if (!(fields >> interfaceName >> destinationText >> gatewayText) ||
          destinationText == "00000000") {
        continue;
      }
      try {
        const auto routeValue = static_cast<std::uint32_t>(
            std::stoul(destinationText, nullptr, 16));
        networks.insert(ntohl(routeValue) & 0xffffff00U);
      } catch (...) {
      }
    }
  }

  for (const std::uint32_t network : networks) {
    for (std::uint32_t suffix = 1; suffix < 255; ++suffix) {
      const std::uint32_t candidate = network | suffix;
      in_addr candidateAddress{};
      candidateAddress.s_addr = htonl(candidate);
      char text[INET_ADDRSTRLEN]{};
      if (inet_ntop(AF_INET, &candidateAddress, text, sizeof(text)) != nullptr) {
        result.emplace(text);
      }
    }
  }

  return result;
}

std::vector<std::string> findOpenGameStreamPorts(
    std::uint16_t port,
    int timeoutMilliseconds) {
  std::vector<PendingConnection> pending;
  std::vector<std::string> connected;

  for (const auto& address : localSubnetAddresses()) {
    const int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
      continue;
    }
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
      close(fd);
      continue;
    }

    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_port = htons(port);
    if (inet_pton(AF_INET, address.c_str(), &endpoint.sin_addr) != 1) {
      close(fd);
      continue;
    }

    const int result = connect(
        fd,
        reinterpret_cast<const sockaddr*>(&endpoint),
        sizeof(endpoint));
    if (result == 0) {
      connected.push_back(address);
      close(fd);
    } else if (errno == EINPROGRESS) {
      pending.push_back({fd, address});
    } else {
      close(fd);
    }
  }

  std::vector<pollfd> descriptors;
  descriptors.reserve(pending.size());
  for (const auto& connection : pending) {
    descriptors.push_back({connection.fd, POLLOUT, 0});
  }

  const auto deadline = std::chrono::steady_clock::now() +
      std::chrono::milliseconds(timeoutMilliseconds);
  std::size_t remaining = descriptors.size();
  while (remaining > 0) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      break;
    }
    const auto waitMilliseconds = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)
            .count());
    if (poll(descriptors.data(), descriptors.size(), waitMilliseconds) <= 0) {
      break;
    }
    for (std::size_t index = 0; index < descriptors.size(); ++index) {
      if (descriptors[index].fd < 0 || descriptors[index].revents == 0) {
        continue;
      }
      int socketError = 0;
      socklen_t errorLength = sizeof(socketError);
      if (getsockopt(
              descriptors[index].fd,
              SOL_SOCKET,
              SO_ERROR,
              &socketError,
              &errorLength) == 0 &&
          socketError == 0) {
        connected.push_back(pending[index].address);
      }
      descriptors[index].fd = -1;
      --remaining;
    }
  }

  for (const auto& connection : pending) {
    close(connection.fd);
  }
  std::sort(connected.begin(), connected.end());
  connected.erase(std::unique(connected.begin(), connected.end()), connected.end());
  return connected;
}

}  // namespace

std::vector<ServerInfo> HostDiscovery::discover(
    std::uint16_t port,
    int timeoutMilliseconds) {
  std::vector<ServerInfo> hosts;
  for (const auto& address :
       findOpenGameStreamPorts(port, timeoutMilliseconds)) {
    try {
      hosts.push_back(ServerInfoClient::fetch(address, port, 1200));
    } catch (...) {
      // An open port is not necessarily a GameStream server.
    }
  }
  return hosts;
}

}  // namespace moonlight::network
