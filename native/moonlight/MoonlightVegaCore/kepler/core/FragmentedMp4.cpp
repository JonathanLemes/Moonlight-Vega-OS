#include "FragmentedMp4.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace moonlight::media {
namespace {

using Bytes = std::vector<std::uint8_t>;

void u8(Bytes& out, std::uint8_t value) { out.push_back(value); }
void u16(Bytes& out, std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>(value >> 8));
  out.push_back(static_cast<std::uint8_t>(value));
}
void u24(Bytes& out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value >> 16));
  out.push_back(static_cast<std::uint8_t>(value >> 8));
  out.push_back(static_cast<std::uint8_t>(value));
}
void u32(Bytes& out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value >> 24));
  out.push_back(static_cast<std::uint8_t>(value >> 16));
  out.push_back(static_cast<std::uint8_t>(value >> 8));
  out.push_back(static_cast<std::uint8_t>(value));
}
void u64(Bytes& out, std::uint64_t value) {
  u32(out, static_cast<std::uint32_t>(value >> 32));
  u32(out, static_cast<std::uint32_t>(value));
}
void fourcc(Bytes& out, const char* type) {
  out.insert(out.end(), type, type + 4);
}
void zeros(Bytes& out, std::size_t count) { out.insert(out.end(), count, 0); }

std::size_t box(Bytes& out, const char* type) {
  const auto start = out.size();
  u32(out, 0);
  fourcc(out, type);
  return start;
}

void finish(Bytes& out, std::size_t start) {
  const auto size = static_cast<std::uint32_t>(out.size() - start);
  out[start] = static_cast<std::uint8_t>(size >> 24);
  out[start + 1] = static_cast<std::uint8_t>(size >> 16);
  out[start + 2] = static_cast<std::uint8_t>(size >> 8);
  out[start + 3] = static_cast<std::uint8_t>(size);
}

void fullBox(Bytes& out, std::uint8_t version, std::uint32_t flags) {
  u8(out, version);
  u24(out, flags);
}

void identityMatrix(Bytes& out) {
  u32(out, 0x00010000); u32(out, 0); u32(out, 0);
  u32(out, 0); u32(out, 0x00010000); u32(out, 0);
  u32(out, 0); u32(out, 0); u32(out, 0x40000000);
}

Bytes fileType() {
  Bytes out;
  const auto ftyp = box(out, "ftyp");
  fourcc(out, "isom");
  u32(out, 0x200);
  fourcc(out, "isom");
  fourcc(out, "iso6");
  fourcc(out, "mp41");
  finish(out, ftyp);
  return out;
}

void movieHeader(Bytes& out, std::uint32_t nextTrackId) {
  const auto mvhd = box(out, "mvhd");
  fullBox(out, 0, 0);
  u32(out, 0); u32(out, 0);
  u32(out, 1000); u32(out, 0);
  u32(out, 0x00010000);
  u16(out, 0x0100); u16(out, 0);
  zeros(out, 8);
  identityMatrix(out);
  zeros(out, 24);
  u32(out, nextTrackId);
  finish(out, mvhd);
}

void trackHeader(
    Bytes& out,
    std::uint32_t trackId,
    bool audio,
    int width,
    int height) {
  const auto tkhd = box(out, "tkhd");
  fullBox(out, 0, 0x000007);
  u32(out, 0); u32(out, 0); u32(out, trackId); u32(out, 0); u32(out, 0);
  zeros(out, 8);
  u16(out, 0); u16(out, 0);
  u16(out, audio ? 0x0100 : 0); u16(out, 0);
  identityMatrix(out);
  u32(out, static_cast<std::uint32_t>(width) << 16);
  u32(out, static_cast<std::uint32_t>(height) << 16);
  finish(out, tkhd);
}

void mediaHeader(Bytes& out, std::uint32_t timescale) {
  const auto mdhd = box(out, "mdhd");
  fullBox(out, 0, 0);
  u32(out, 0); u32(out, 0); u32(out, timescale); u32(out, 0);
  u16(out, 0x55c4);  // und
  u16(out, 0);
  finish(out, mdhd);
}

void handler(Bytes& out, bool audio) {
  const auto hdlr = box(out, "hdlr");
  fullBox(out, 0, 0);
  u32(out, 0);
  fourcc(out, audio ? "soun" : "vide");
  zeros(out, 12);
  const char* name = audio ? "Moonlight Opus" : "Moonlight Video";
  while (*name) u8(out, static_cast<std::uint8_t>(*name++));
  u8(out, 0);
  finish(out, hdlr);
}

void dataInfo(Bytes& out) {
  const auto dinf = box(out, "dinf");
  const auto dref = box(out, "dref");
  fullBox(out, 0, 0);
  u32(out, 1);
  const auto url = box(out, "url ");
  fullBox(out, 0, 1);
  finish(out, url);
  finish(out, dref);
  finish(out, dinf);
}

void emptySampleTables(Bytes& out) {
  for (const char* type : {"stts", "stsc", "stsz", "stco"}) {
    const auto item = box(out, type);
    fullBox(out, 0, 0);
    u32(out, 0);
    if (std::string(type) == "stsz") u32(out, 0);
    finish(out, item);
  }
}

Bytes videoInit(
    int width,
    int height,
    const Bytes& sps,
    const Bytes& pps) {
  if (sps.size() < 4 || pps.empty()) {
    throw std::runtime_error("H.264 stream did not provide SPS/PPS configuration");
  }
  Bytes out = fileType();
  const auto moov = box(out, "moov");
  movieHeader(out, 2);
  const auto trak = box(out, "trak");
  trackHeader(out, 1, false, width, height);
  const auto mdia = box(out, "mdia");
  mediaHeader(out, 90000);
  handler(out, false);
  const auto minf = box(out, "minf");
  const auto vmhd = box(out, "vmhd");
  fullBox(out, 0, 1);
  zeros(out, 8);
  finish(out, vmhd);
  dataInfo(out);
  const auto stbl = box(out, "stbl");
  const auto stsd = box(out, "stsd");
  fullBox(out, 0, 0);
  u32(out, 1);
  const auto avc1 = box(out, "avc1");
  zeros(out, 6); u16(out, 1);
  zeros(out, 16);
  u16(out, static_cast<std::uint16_t>(width));
  u16(out, static_cast<std::uint16_t>(height));
  u32(out, 0x00480000); u32(out, 0x00480000);
  u32(out, 0); u16(out, 1);
  zeros(out, 32);
  u16(out, 0x0018); u16(out, 0xffff);
  const auto avcC = box(out, "avcC");
  u8(out, 1); u8(out, sps[1]); u8(out, sps[2]); u8(out, sps[3]);
  u8(out, 0xff); u8(out, 0xe1);
  u16(out, static_cast<std::uint16_t>(sps.size()));
  out.insert(out.end(), sps.begin(), sps.end());
  u8(out, 1);
  u16(out, static_cast<std::uint16_t>(pps.size()));
  out.insert(out.end(), pps.begin(), pps.end());
  finish(out, avcC);
  finish(out, avc1);
  finish(out, stsd);
  emptySampleTables(out);
  finish(out, stbl);
  finish(out, minf);
  finish(out, mdia);
  finish(out, trak);
  const auto mvex = box(out, "mvex");
  const auto trex = box(out, "trex");
  fullBox(out, 0, 0);
  u32(out, 1); u32(out, 1); u32(out, 0); u32(out, 0); u32(out, 0x01010000);
  finish(out, trex);
  finish(out, mvex);
  finish(out, moov);
  return out;
}

Bytes audioInit(int sampleRate, int channels) {
  Bytes out = fileType();
  const auto moov = box(out, "moov");
  movieHeader(out, 3);
  const auto trak = box(out, "trak");
  trackHeader(out, 2, true, 0, 0);
  const auto mdia = box(out, "mdia");
  mediaHeader(out, static_cast<std::uint32_t>(sampleRate));
  handler(out, true);
  const auto minf = box(out, "minf");
  const auto smhd = box(out, "smhd");
  fullBox(out, 0, 0); u16(out, 0); u16(out, 0);
  finish(out, smhd);
  dataInfo(out);
  const auto stbl = box(out, "stbl");
  const auto stsd = box(out, "stsd");
  fullBox(out, 0, 0); u32(out, 1);
  const auto opus = box(out, "Opus");
  zeros(out, 6); u16(out, 1);
  zeros(out, 8);
  u16(out, static_cast<std::uint16_t>(channels));
  u16(out, 16); u16(out, 0); u16(out, 0);
  u32(out, static_cast<std::uint32_t>(sampleRate) << 16);
  const auto dops = box(out, "dOps");
  u8(out, 0);
  u8(out, static_cast<std::uint8_t>(channels));
  u16(out, 0);
  u32(out, static_cast<std::uint32_t>(sampleRate));
  u16(out, 0);
  u8(out, 0);
  finish(out, dops);
  finish(out, opus);
  finish(out, stsd);
  emptySampleTables(out);
  finish(out, stbl);
  finish(out, minf);
  finish(out, mdia);
  finish(out, trak);
  const auto mvex = box(out, "mvex");
  const auto trex = box(out, "trex");
  fullBox(out, 0, 0);
  u32(out, 2); u32(out, 1); u32(out, 0); u32(out, 0); u32(out, 0);
  finish(out, trex);
  finish(out, mvex);
  finish(out, moov);
  return out;
}

Bytes fragment(
    std::uint32_t trackId,
    std::uint32_t sequence,
    std::uint64_t decodeTime,
    std::uint32_t duration,
    std::uint32_t flags,
    const Bytes& sample) {
  Bytes out;
  const auto moof = box(out, "moof");
  const auto mfhd = box(out, "mfhd");
  fullBox(out, 0, 0); u32(out, sequence); finish(out, mfhd);
  const auto traf = box(out, "traf");
  const auto tfhd = box(out, "tfhd");
  fullBox(out, 0, 0x020000); u32(out, trackId); finish(out, tfhd);
  const auto tfdt = box(out, "tfdt");
  fullBox(out, 1, 0); u64(out, decodeTime); finish(out, tfdt);
  const auto trun = box(out, "trun");
  fullBox(out, 0, 0x000701);
  u32(out, 1);
  const auto dataOffsetPosition = out.size();
  u32(out, 0);
  u32(out, duration);
  u32(out, static_cast<std::uint32_t>(sample.size()));
  u32(out, flags);
  finish(out, trun);
  finish(out, traf);
  finish(out, moof);
  const auto dataOffset = static_cast<std::uint32_t>(out.size() + 8);
  out[dataOffsetPosition] = static_cast<std::uint8_t>(dataOffset >> 24);
  out[dataOffsetPosition + 1] = static_cast<std::uint8_t>(dataOffset >> 16);
  out[dataOffsetPosition + 2] = static_cast<std::uint8_t>(dataOffset >> 8);
  out[dataOffsetPosition + 3] = static_cast<std::uint8_t>(dataOffset);
  const auto mdat = box(out, "mdat");
  out.insert(out.end(), sample.begin(), sample.end());
  finish(out, mdat);
  return out;
}

std::size_t startCodeSize(const Bytes& data, std::size_t at) {
  if (at + 3 <= data.size() && data[at] == 0 && data[at + 1] == 0 &&
      data[at + 2] == 1) return 3;
  if (at + 4 <= data.size() && data[at] == 0 && data[at + 1] == 0 &&
      data[at + 2] == 0 && data[at + 3] == 1) return 4;
  return 0;
}

std::vector<Bytes> annexBNalus(const DECODE_UNIT& unit) {
  Bytes data;
  data.reserve(static_cast<std::size_t>(unit.fullLength));
  for (auto* entry = unit.bufferList; entry; entry = entry->next) {
    data.insert(
        data.end(),
        reinterpret_cast<std::uint8_t*>(entry->data),
        reinterpret_cast<std::uint8_t*>(entry->data) + entry->length);
  }
  std::vector<Bytes> nalus;
  std::size_t position = 0;
  while (position < data.size() && startCodeSize(data, position) == 0) ++position;
  while (position < data.size()) {
    const auto prefix = startCodeSize(data, position);
    if (prefix == 0) { ++position; continue; }
    const auto begin = position + prefix;
    auto end = begin;
    while (end < data.size() && startCodeSize(data, end) == 0) ++end;
    if (end > begin) nalus.emplace_back(data.begin() + begin, data.begin() + end);
    position = end;
  }
  if (nalus.empty() && !data.empty()) nalus.push_back(std::move(data));
  return nalus;
}

}  // namespace

void FragmentedMp4::configureVideo(int width, int height, int fps) {
  width_ = width;
  height_ = height;
  fps_ = std::max(1, fps);
  videoSequence_ = 1;
  videoDecodeTime_ = 0;
  videoInitialized_ = false;
}

std::vector<MediaChunk> FragmentedMp4::videoFrame(const DECODE_UNIT& unit) {
  auto nalus = annexBNalus(unit);
  Bytes sps;
  Bytes pps;
  Bytes sample;
  for (const auto& nalu : nalus) {
    if (nalu.empty()) continue;
    const auto type = nalu[0] & 0x1f;
    if (type == 7) sps = nalu;
    if (type == 8) pps = nalu;
    u32(sample, static_cast<std::uint32_t>(nalu.size()));
    sample.insert(sample.end(), nalu.begin(), nalu.end());
  }
  std::vector<MediaChunk> chunks;
  if (!videoInitialized_) {
    if (sps.empty() || pps.empty()) return chunks;
    std::ostringstream codec;
    codec << "video/mp4; codecs=\"avc1." << std::hex;
    const char digits[] = "0123456789ABCDEF";
    for (std::size_t i = 1; i < 4; ++i) {
      codec << digits[sps[i] >> 4] << digits[sps[i] & 0x0f];
    }
    codec << '"';
    chunks.push_back(MediaChunk{"video-init:" + codec.str(), videoInit(width_, height_, sps, pps)});
    videoInitialized_ = true;
  }
  if (sample.empty()) return chunks;
  const auto duration = static_cast<std::uint32_t>(90000 / fps_);
  const auto sampleFlags = unit.frameType == FRAME_TYPE_IDR
      ? 0x02000000U
      : 0x01010000U;
  chunks.push_back(MediaChunk{
      "video",
      fragment(1, videoSequence_++, videoDecodeTime_, duration, sampleFlags, sample)});
  videoDecodeTime_ += duration;
  return chunks;
}

MediaChunk FragmentedMp4::configureAudio(
    const OPUS_MULTISTREAM_CONFIGURATION& config) {
  audioSampleRate_ = config.sampleRate;
  audioChannels_ = config.channelCount;
  audioSamplesPerFrame_ = config.samplesPerFrame;
  audioSequence_ = 1;
  audioDecodeTime_ = 0;
  audioInitialized_ = true;
  return MediaChunk{
      "audio-init:audio/mp4; codecs=\"opus\"",
      audioInit(audioSampleRate_, audioChannels_)};
}

MediaChunk FragmentedMp4::audioPacket(const char* data, int size) {
  if (!audioInitialized_ || size <= 0) return MediaChunk{"audio", {}};
  Bytes sample(
      reinterpret_cast<const std::uint8_t*>(data),
      reinterpret_cast<const std::uint8_t*>(data) + size);
  auto bytes = fragment(
      2,
      audioSequence_++,
      audioDecodeTime_,
      static_cast<std::uint32_t>(audioSamplesPerFrame_),
      0,
      sample);
  audioDecodeTime_ += static_cast<std::uint32_t>(audioSamplesPerFrame_);
  return MediaChunk{"audio", std::move(bytes)};
}

}  // namespace moonlight::media
