#pragma once

#include "FragmentedMp4.h"
#include "GameStreamClient.h"

#include <Limelight.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace moonlight::streaming {

using StreamEventHandler =
    std::function<void(const std::string&, std::vector<std::uint8_t>)>;

class StreamSession {
 public:
  explicit StreamSession(StreamEventHandler eventHandler);
  ~StreamSession();

  StreamSession(const StreamSession&) = delete;
  StreamSession& operator=(const StreamSession&) = delete;

  void start(
      const std::string& host,
      const network::ServerInfo& server,
      gamestream::LaunchResult launch);
  void stop();
  bool running() const { return running_.load(); }
  void emit(std::string event, std::vector<std::uint8_t> bytes = {});
  void markTerminated() { running_.store(false); }

  void sendController(
      int buttons,
      int leftTrigger,
      int rightTrigger,
      int leftStickX,
      int leftStickY,
      int rightStickX,
      int rightStickY);

 private:
  friend int videoSetup(int, int, int, int, void*, int);
  friend int videoSubmit(PDECODE_UNIT);
  friend int audioInit(int, const POPUS_MULTISTREAM_CONFIGURATION, void*, int);
  friend void audioSubmit(char*, int);

  int onVideoSetup(int videoFormat, int width, int height, int fps);
  int onVideoFrame(PDECODE_UNIT unit);
  int onAudioInit(const POPUS_MULTISTREAM_CONFIGURATION config);
  void onAudioPacket(char* data, int size);

  StreamEventHandler eventHandler_;
  media::FragmentedMp4 muxer_;
  std::atomic<bool> running_{false};
  std::mutex lifecycleMutex_;

  std::string host_;
  std::string appVersion_;
  std::string gsVersion_;
  std::string rtspUrl_;
  SERVER_INFORMATION serverInformation_{};
  STREAM_CONFIGURATION configuration_{};
  CONNECTION_LISTENER_CALLBACKS connectionCallbacks_{};
  DECODER_RENDERER_CALLBACKS videoCallbacks_{};
  AUDIO_RENDERER_CALLBACKS audioCallbacks_{};
};

}  // namespace moonlight::streaming
