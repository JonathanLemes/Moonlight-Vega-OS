#pragma once

#include <Limelight.h>

#include <cstdint>
#include <string>
#include <vector>

namespace moonlight::media {

struct MediaChunk {
  std::string event;
  std::vector<std::uint8_t> bytes;
};

class FragmentedMp4 {
 public:
  void configureVideo(int width, int height, int fps);
  std::vector<MediaChunk> videoFrame(const DECODE_UNIT& unit);

  MediaChunk configureAudio(const OPUS_MULTISTREAM_CONFIGURATION& config);
  MediaChunk audioPacket(const char* data, int size);

 private:
  int width_{1920};
  int height_{1080};
  int fps_{60};
  std::uint32_t videoSequence_{1};
  std::uint64_t videoDecodeTime_{0};
  bool videoInitialized_{false};

  int audioSampleRate_{48000};
  int audioChannels_{2};
  int audioSamplesPerFrame_{240};
  std::uint32_t audioSequence_{1};
  std::uint64_t audioDecodeTime_{0};
  bool audioInitialized_{false};
};

}  // namespace moonlight::media
