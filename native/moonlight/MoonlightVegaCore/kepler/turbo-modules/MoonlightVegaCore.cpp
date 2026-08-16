#include "MoonlightVegaCore.h"

#include "core/ServerInfoClient.h"

#include <Limelight.h>

#include <cstdint>
#include <memory>
#include <stdexcept>

using namespace com::amazon::kepler::turbomodule;

namespace MoonlightVegaCoreTurboModule {

MoonlightVegaCore::MoonlightVegaCore() = default;
MoonlightVegaCore::~MoonlightVegaCore() noexcept = default;

JSObject MoonlightVegaCore::getCoreInfo() {
  STREAM_CONFIGURATION configuration;
  LiInitializeStreamConfiguration(&configuration);

  JSObject info;
  info["moonlightCommonLinked"] = true;
  info["defaultWidth"] = static_cast<std::int32_t>(1920);
  info["defaultHeight"] = static_cast<std::int32_t>(1080);
  info["defaultFps"] = static_cast<std::int32_t>(60);
  info["launchQueryParameters"] = std::string(LiGetLaunchUrlQueryParameters());
  return info;
}

Promise MoonlightVegaCore::getServerInfo(std::string host, double port) {
  return Promise([host = std::move(host), port](std::shared_ptr<Promise> promise) {
    try {
      if (port < 1 || port > 65535) {
        throw std::invalid_argument("Port must be between 1 and 65535");
      }

      const auto result = moonlight::network::ServerInfoClient::fetch(
          host,
          static_cast<std::uint16_t>(port));

      JSObject info;
      info["address"] = result.address;
      info["port"] = static_cast<std::int32_t>(result.port);
      info["hostname"] = result.hostname;
      info["appVersion"] = result.appVersion;
      info["gsVersion"] = result.gsVersion;
      info["uniqueId"] = result.uniqueId;
      info["state"] = result.state;
      info["paired"] = result.paired;
      info["currentGame"] = result.currentGame;
      info["serverCodecModeSupport"] = result.serverCodecModeSupport;
      promise->resolve(info);
    } catch (const std::exception& error) {
      promise->reject(error.what());
    }
  });
}

}  // namespace MoonlightVegaCoreTurboModule

