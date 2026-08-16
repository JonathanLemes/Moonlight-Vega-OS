#include "StreamSession.h"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include <utility>

namespace moonlight::streaming {
namespace {

std::mutex gSessionMutex;
StreamSession* gSession = nullptr;

StreamSession* activeSession() {
  std::lock_guard<std::mutex> lock(gSessionMutex);
  return gSession;
}

void stageStarting(int stage) {
  if (auto* session = activeSession()) {
    session->emit("status:Connecting: " + std::string(LiGetStageName(stage)));
  }
}

void stageComplete(int stage) {
  if (auto* session = activeSession()) {
    session->emit("status:Ready: " + std::string(LiGetStageName(stage)));
  }
}

void stageFailed(int stage, int errorCode) {
  if (auto* session = activeSession()) {
    session->emit(
        "error:" + std::string(LiGetStageName(stage)) + " failed (" +
        std::to_string(errorCode) + ")");
  }
}

void connectionStarted() {
  if (auto* session = activeSession()) {
    session->emit("status:Streaming");
    LiSendControllerArrivalEvent(
        0,
        1,
        LI_CTYPE_XBOX,
        A_FLAG | B_FLAG | X_FLAG | Y_FLAG | UP_FLAG | DOWN_FLAG | LEFT_FLAG |
            RIGHT_FLAG | LB_FLAG | RB_FLAG | PLAY_FLAG | BACK_FLAG |
            LS_CLK_FLAG | RS_CLK_FLAG | SPECIAL_FLAG,
        LI_CCAP_ANALOG_TRIGGERS | LI_CCAP_RUMBLE);
  }
}

void connectionTerminated(int errorCode) {
  if (auto* session = activeSession()) {
    session->markTerminated();
    session->emit(
        errorCode == 0
            ? "status:Stream ended"
            : "error:Stream ended unexpectedly (" + std::to_string(errorCode) +
                ")");
  }
}

void logMessage(const char* format, ...) {
  std::array<char, 1024> message{};
  va_list arguments;
  va_start(arguments, format);
  std::vsnprintf(message.data(), message.size(), format, arguments);
  va_end(arguments);
  if (auto* session = activeSession()) {
    session->emit("log:" + std::string(message.data()));
  }
}

void rendererStart() {}
void rendererStop() {}
void rendererCleanup() {}

}  // namespace

int videoSetup(
    int videoFormat,
    int width,
    int height,
    int redrawRate,
    void*,
    int) {
  auto* session = activeSession();
  return session ? session->onVideoSetup(
      videoFormat, width, height, redrawRate) : -1;
}

int videoSubmit(PDECODE_UNIT unit) {
  auto* session = activeSession();
  return session ? session->onVideoFrame(unit) : DR_NEED_IDR;
}

int audioInit(
    int,
    const POPUS_MULTISTREAM_CONFIGURATION config,
    void*,
    int) {
  auto* session = activeSession();
  return session ? session->onAudioInit(config) : -1;
}

void audioSubmit(char* data, int size) {
  if (auto* session = activeSession()) session->onAudioPacket(data, size);
}

StreamSession::StreamSession(StreamEventHandler eventHandler)
    : eventHandler_(std::move(eventHandler)) {
  LiInitializeServerInformation(&serverInformation_);
  LiInitializeStreamConfiguration(&configuration_);
  LiInitializeConnectionCallbacks(&connectionCallbacks_);
  LiInitializeVideoCallbacks(&videoCallbacks_);
  LiInitializeAudioCallbacks(&audioCallbacks_);

  connectionCallbacks_.stageStarting = stageStarting;
  connectionCallbacks_.stageComplete = stageComplete;
  connectionCallbacks_.stageFailed = stageFailed;
  connectionCallbacks_.connectionStarted = connectionStarted;
  connectionCallbacks_.connectionTerminated = connectionTerminated;
  connectionCallbacks_.logMessage = logMessage;

  videoCallbacks_.setup = videoSetup;
  videoCallbacks_.start = rendererStart;
  videoCallbacks_.stop = rendererStop;
  videoCallbacks_.cleanup = rendererCleanup;
  videoCallbacks_.submitDecodeUnit = videoSubmit;
  videoCallbacks_.capabilities = CAPABILITY_DIRECT_SUBMIT;

  audioCallbacks_.init = audioInit;
  audioCallbacks_.start = rendererStart;
  audioCallbacks_.stop = rendererStop;
  audioCallbacks_.cleanup = rendererCleanup;
  audioCallbacks_.decodeAndPlaySample = audioSubmit;
  audioCallbacks_.capabilities =
      CAPABILITY_DIRECT_SUBMIT | CAPABILITY_SUPPORTS_ARBITRARY_AUDIO_DURATION;
}

StreamSession::~StreamSession() {
  stop();
}

void StreamSession::start(
    const std::string& host,
    const network::ServerInfo& server,
    gamestream::LaunchResult launch) {
  std::lock_guard<std::mutex> lifecycleLock(lifecycleMutex_);
  if (running_.load()) throw std::runtime_error("A stream is already running");
  {
    std::lock_guard<std::mutex> globalLock(gSessionMutex);
    if (gSession && gSession != this) {
      throw std::runtime_error("Another Moonlight stream is already active");
    }
    gSession = this;
  }
  host_ = host;
  appVersion_ = server.appVersion;
  gsVersion_ = server.gsVersion;
  rtspUrl_ = std::move(launch.rtspUrl);
  configuration_ = launch.configuration;
  serverInformation_.address = host_.c_str();
  serverInformation_.serverInfoAppVersion = appVersion_.c_str();
  serverInformation_.serverInfoGfeVersion = gsVersion_.c_str();
  serverInformation_.rtspSessionUrl = rtspUrl_.c_str();
  serverInformation_.serverCodecModeSupport =
      static_cast<int>(server.serverCodecModeSupport);
  running_.store(true);
  const int result = LiStartConnection(
      &serverInformation_,
      &configuration_,
      &connectionCallbacks_,
      &videoCallbacks_,
      &audioCallbacks_,
      this,
      0,
      this,
      0);
  if (result != 0) {
    running_.store(false);
    std::lock_guard<std::mutex> globalLock(gSessionMutex);
    if (gSession == this) gSession = nullptr;
    throw std::runtime_error(
        "Moonlight connection failed with code " + std::to_string(result));
  }
}

void StreamSession::stop() {
  std::lock_guard<std::mutex> lifecycleLock(lifecycleMutex_);
  if (running_.exchange(false)) LiStopConnection();
  std::lock_guard<std::mutex> globalLock(gSessionMutex);
  if (gSession == this) gSession = nullptr;
}

void StreamSession::sendController(
    int buttons,
    int leftTrigger,
    int rightTrigger,
    int leftStickX,
    int leftStickY,
    int rightStickX,
    int rightStickY) {
  if (!running_.load()) return;
  LiSendMultiControllerEvent(
      0,
      1,
      buttons,
      static_cast<unsigned char>(std::clamp(leftTrigger, 0, 255)),
      static_cast<unsigned char>(std::clamp(rightTrigger, 0, 255)),
      static_cast<short>(std::clamp(leftStickX, -32768, 32767)),
      static_cast<short>(std::clamp(leftStickY, -32768, 32767)),
      static_cast<short>(std::clamp(rightStickX, -32768, 32767)),
      static_cast<short>(std::clamp(rightStickY, -32768, 32767)));
}

void StreamSession::emit(
    std::string event,
    std::vector<std::uint8_t> bytes) {
  if (eventHandler_) eventHandler_(event, std::move(bytes));
}

int StreamSession::onVideoSetup(
    int videoFormat,
    int width,
    int height,
    int fps) {
  if ((videoFormat & VIDEO_FORMAT_MASK_H264) == 0) {
    emit("error:This build currently requires H.264 for Vega Media Source");
    return -1;
  }
  muxer_.configureVideo(width, height, fps);
  return 0;
}

int StreamSession::onVideoFrame(PDECODE_UNIT unit) {
  try {
    for (auto& chunk : muxer_.videoFrame(*unit)) {
      emit(std::move(chunk.event), std::move(chunk.bytes));
    }
    return DR_OK;
  } catch (const std::exception& error) {
    emit("error:Video packaging failed: " + std::string(error.what()));
    return DR_NEED_IDR;
  }
}

int StreamSession::onAudioInit(
    const POPUS_MULTISTREAM_CONFIGURATION config) {
  if (!config || config->channelCount > 2) {
    emit("error:Only stereo Opus audio is supported");
    return -1;
  }
  auto chunk = muxer_.configureAudio(*config);
  emit(std::move(chunk.event), std::move(chunk.bytes));
  return 0;
}

void StreamSession::onAudioPacket(char* data, int size) {
  auto chunk = muxer_.audioPacket(data, size);
  if (!chunk.bytes.empty()) {
    emit(std::move(chunk.event), std::move(chunk.bytes));
  }
}

}  // namespace moonlight::streaming
