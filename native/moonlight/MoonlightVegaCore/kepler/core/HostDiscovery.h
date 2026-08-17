#pragma once

#include "ServerInfoClient.h"

#include <cstdint>
#include <vector>

namespace moonlight::network {

class HostDiscovery {
 public:
  static std::vector<ServerInfo> discover(
      std::uint16_t port = 47989,
      int timeoutMilliseconds = 900);
};

}  // namespace moonlight::network
