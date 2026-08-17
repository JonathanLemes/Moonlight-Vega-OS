#include "MoonlightVegaCore.h"

#include "core/ServerInfoClient.h"
#include "core/GameStreamClient.h"
#include "core/HostDiscovery.h"

#include <Limelight.h>

#include <cstdint>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

using namespace com::amazon::kepler::turbomodule;

namespace MoonlightVegaCoreTurboModule {

namespace {

std::uint16_t checkedPort(double port) {
  if (!std::isfinite(port) || port < 1 || port > 65535) {
    throw std::invalid_argument("Port must be between 1 and 65535");
  }
  return static_cast<std::uint16_t>(port);
}

std::int32_t checkedInt(double value, const char* name) {
  if (!std::isfinite(value) || value < 0 || value > 2147483647.0) {
    throw std::invalid_argument(std::string("Invalid ") + name);
  }
  return static_cast<std::int32_t>(value);
}

JSObject serverInfoObject(const moonlight::network::ServerInfo& result) {
  JSObject info;
  info["address"] = result.address;
  info["port"] = static_cast<std::int32_t>(result.port);
  info["httpsPort"] = static_cast<std::int32_t>(result.httpsPort);
  info["hostname"] = result.hostname;
  info["appVersion"] = result.appVersion;
  info["gsVersion"] = result.gsVersion;
  info["uniqueId"] = result.uniqueId;
  info["state"] = result.state;
  info["paired"] = result.paired;
  info["currentGame"] = result.currentGame;
  info["serverCodecModeSupport"] = result.serverCodecModeSupport;
  return info;
}

}  // namespace

MoonlightVegaCore::MoonlightVegaCore() = default;
MoonlightVegaCore::~MoonlightVegaCore() noexcept {
  try {
    std::shared_ptr<moonlight::streaming::StreamSession> session;
    {
      std::lock_guard<std::mutex> lock(stateMutex_);
      session = std::move(streamSession_);
    }
    if (session) session->stop();
  } catch (...) {
  }
}

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

Promise MoonlightVegaCore::discoverHosts() {
  return Promise([](std::shared_ptr<Promise> promise) {
    try {
      JSArray hosts;
      for (const auto& host : moonlight::network::HostDiscovery::discover()) {
        hosts.emplace_back(serverInfoObject(host));
      }
      JSObject result;
      result["hosts"] = std::move(hosts);
      promise->resolve(result);
    } catch (const std::exception& error) {
      promise->reject(error.what());
    }
  });
}

Promise MoonlightVegaCore::getServerInfo(std::string host, double port) {
  return Promise([host = std::move(host), port](std::shared_ptr<Promise> promise) {
    try {
      moonlight::gamestream::GameStreamClient client(host, checkedPort(port));
      const auto result = client.serverInfo();
      promise->resolve(serverInfoObject(result));
    } catch (const std::exception& error) {
      promise->reject(error.what());
    }
  });
}

Promise MoonlightVegaCore::pair(
    std::string host,
    double port,
    std::string pin) {
  return Promise([
      host = std::move(host),
      port,
      pin = std::move(pin)](std::shared_ptr<Promise> promise) {
    try {
      moonlight::gamestream::GameStreamClient client(host, checkedPort(port));
      client.pair(pin);
      JSObject result;
      result["paired"] = true;
      promise->resolve(result);
    } catch (const std::exception& error) {
      promise->reject(error.what());
    }
  });
}

Promise MoonlightVegaCore::getApps(std::string host, double port) {
  return Promise([
      host = std::move(host),
      port](std::shared_ptr<Promise> promise) {
    try {
      moonlight::gamestream::GameStreamClient client(host, checkedPort(port));
      JSArray apps;
      for (const auto& app : client.apps()) {
        JSObject item;
        item["id"] = app.id;
        item["name"] = app.name;
        apps.emplace_back(std::move(item));
      }
      JSObject result;
      result["apps"] = std::move(apps);
      promise->resolve(result);
    } catch (const std::exception& error) {
      promise->reject(error.what());
    }
  });
}

void MoonlightVegaCore::setStreamEventHandler(Callback handler) {
  std::lock_guard<std::mutex> lock(stateMutex_);
  streamCallback_ = std::make_shared<Callback>(std::move(handler));
}

Promise MoonlightVegaCore::startStream(
    std::string host,
    double port,
    double appId,
    double width,
    double height,
    double fps,
    double bitrateKbps,
    std::string codec) {
  return Promise([
      this,
      host = std::move(host),
      port,
      appId,
      width,
      height,
      fps,
      bitrateKbps,
      codec = std::move(codec)](std::shared_ptr<Promise> promise) {
    try {
      std::shared_ptr<Callback> callback;
      {
        std::lock_guard<std::mutex> lock(stateMutex_);
        callback = streamCallback_;
        if (streamSession_ && streamSession_->running()) {
          throw std::runtime_error("A stream is already running");
        }
      }
      if (!callback) {
        throw std::runtime_error("Set the stream event handler before starting");
      }
      if (codec != "h264") {
        throw std::invalid_argument(
            "H.264 is required for the initial Vega media backend");
      }
      moonlight::gamestream::GameStreamClient client(host, checkedPort(port));
      const auto server = client.serverInfo();
      auto launch = client.launch(
          checkedInt(appId, "app ID"),
          checkedInt(width, "width"),
          checkedInt(height, "height"),
          checkedInt(fps, "frame rate"),
          checkedInt(bitrateKbps, "bitrate"),
          codec);
      auto session = std::make_shared<moonlight::streaming::StreamSession>(
          [callback](const std::string& event, std::vector<std::uint8_t> bytes) {
            try {
              auto storage = std::make_shared<std::vector<std::uint8_t>>(
                  std::move(bytes));
              callback->invoke(event, ArrayBuffer(std::move(storage)));
            } catch (...) {
            }
          });
      {
        std::lock_guard<std::mutex> lock(stateMutex_);
        streamSession_ = session;
      }
      session->start(host, server, std::move(launch));
      JSObject result;
      result["streaming"] = true;
      promise->resolve(result);
    } catch (const std::exception& error) {
      promise->reject(error.what());
    }
  });
}

Promise MoonlightVegaCore::stopStream(
    std::string host,
    double port,
    bool quitApp) {
  return Promise([
      this,
      host = std::move(host),
      port,
      quitApp](std::shared_ptr<Promise> promise) {
    try {
      std::shared_ptr<moonlight::streaming::StreamSession> session;
      {
        std::lock_guard<std::mutex> lock(stateMutex_);
        session = std::move(streamSession_);
      }
      if (session) session->stop();
      if (quitApp) {
        moonlight::gamestream::GameStreamClient client(
            host, checkedPort(port));
        client.quit();
      }
      JSObject result;
      result["streaming"] = false;
      promise->resolve(result);
    } catch (const std::exception& error) {
      promise->reject(error.what());
    }
  });
}

void MoonlightVegaCore::sendControllerState(
    double buttons,
    double leftTrigger,
    double rightTrigger,
    double leftStickX,
    double leftStickY,
    double rightStickX,
    double rightStickY) {
  std::shared_ptr<moonlight::streaming::StreamSession> session;
  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    session = streamSession_;
  }
  if (session) {
    session->sendController(
        static_cast<int>(buttons),
        static_cast<int>(leftTrigger),
        static_cast<int>(rightTrigger),
        static_cast<int>(leftStickX),
        static_cast<int>(leftStickY),
        static_cast<int>(rightStickX),
        static_cast<int>(rightStickY));
  }
}

}  // namespace MoonlightVegaCoreTurboModule
