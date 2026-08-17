#pragma once

#include <cstdint>
#include <string>

namespace moonlight::network {

struct ServerInfo {
  std::string address;
  std::uint16_t port;
  std::string hostname;
  std::string appVersion;
  std::string gsVersion;
  std::string uniqueId;
  std::string state;
  std::uint16_t httpsPort;
  bool paired;
  std::int32_t currentGame;
  double serverCodecModeSupport;
};

class ServerInfoClient {
 public:
  static ServerInfo fetch(
      const std::string& host,
      std::uint16_t port,
      int timeoutMilliseconds = 5000,
      const std::string& clientUniqueId = "0123456789ABCDEF");
};

}  // namespace moonlight::network
