#pragma once

#include "ServerInfoClient.h"

#include <Limelight.h>

#include <cstdint>
#include <string>
#include <vector>

namespace moonlight::gamestream {

struct App {
  std::int32_t id;
  std::string name;
};

struct LaunchResult {
  std::string rtspUrl;
  STREAM_CONFIGURATION configuration;
};

class GameStreamClient {
 public:
  GameStreamClient(std::string host, std::uint16_t httpPort = 47989);

  network::ServerInfo serverInfo() const;
  void pair(const std::string& pin);
  std::vector<App> apps() const;
  LaunchResult launch(
      std::int32_t appId,
      std::int32_t width,
      std::int32_t height,
      std::int32_t fps,
      std::int32_t bitrateKbps,
      const std::string& codec) const;
  void quit() const;

 private:
  std::string host_;
  std::uint16_t httpPort_;
};

}  // namespace moonlight::gamestream
