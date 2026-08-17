#include "../kepler/core/HostDiscovery.h"

#include <iostream>

int main() {
  const auto hosts = moonlight::network::HostDiscovery::discover();
  for (const auto& host : hosts) {
    std::cout << host.address << '\t' << host.hostname << '\n';
  }
  return hosts.empty() ? 1 : 0;
}
