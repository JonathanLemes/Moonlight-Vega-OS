#pragma once

#include "generated/MoonlightVegaCoreSpec.h"

#include "core/StreamSession.h"

#include <memory>
#include <mutex>

namespace MoonlightVegaCoreTurboModule {

class MoonlightVegaCore : public MoonlightVegaCoreSpec {
 public:
  MoonlightVegaCore();
  ~MoonlightVegaCore() noexcept override;

  com::amazon::kepler::turbomodule::JSObject getCoreInfo() override;
  com::amazon::kepler::turbomodule::Promise discoverHosts() override;
  com::amazon::kepler::turbomodule::Promise getServerInfo(
      std::string host,
      double port) override;
  com::amazon::kepler::turbomodule::Promise pair(
      std::string host,
      double port,
      std::string pin) override;
  com::amazon::kepler::turbomodule::Promise getApps(
      std::string host,
      double port) override;
  void setStreamEventHandler(
      com::amazon::kepler::turbomodule::Callback handler) override;
  com::amazon::kepler::turbomodule::Promise startStream(
      std::string host,
      double port,
      double appId,
      double width,
      double height,
      double fps,
      double bitrateKbps,
      std::string codec) override;
  com::amazon::kepler::turbomodule::Promise stopStream(
      std::string host,
      double port,
      bool quitApp) override;
  void sendControllerState(
      double buttons,
      double leftTrigger,
      double rightTrigger,
      double leftStickX,
      double leftStickY,
      double rightStickX,
      double rightStickY) override;

 private:
  std::mutex stateMutex_;
  std::shared_ptr<com::amazon::kepler::turbomodule::Callback> streamCallback_;
  std::shared_ptr<moonlight::streaming::StreamSession> streamSession_;
};

}  // namespace MoonlightVegaCoreTurboModule
