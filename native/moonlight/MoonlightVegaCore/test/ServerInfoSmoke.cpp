#include "../kepler/core/ServerInfoClient.h"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <string>

namespace {

std::uint16_t parsePort(const char* value) {
  const unsigned long port = std::stoul(value);
  if (port == 0 || port > std::numeric_limits<std::uint16_t>::max()) {
    throw std::invalid_argument("Port must be between 1 and 65535");
  }
  return static_cast<std::uint16_t>(port);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: moonlight-vega-serverinfo-smoke <host> [port]\n";
    return EXIT_FAILURE;
  }

  try {
    const std::uint16_t port = argc == 3 ? parsePort(argv[2]) : 47989;
    const auto info = moonlight::network::ServerInfoClient::fetch(argv[1], port);

    std::cout << "hostname=" << info.hostname << '\n'
              << "address=" << info.address << ':' << info.port << '\n'
              << "state=" << info.state << '\n'
              << "appVersion=" << info.appVersion << '\n'
              << "gsVersion=" << info.gsVersion << '\n'
              << "paired=" << (info.paired ? "true" : "false") << '\n'
              << "currentGame=" << info.currentGame << '\n'
              << "serverCodecModeSupport=" << info.serverCodecModeSupport << '\n';

    if (info.hostname.empty()) {
      std::cerr << "The response did not include a hostname\n";
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
