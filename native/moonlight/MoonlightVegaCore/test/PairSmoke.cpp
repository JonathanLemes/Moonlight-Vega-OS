#include "../kepler/core/GameStreamClient.h"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "Usage: PairSmoke HOST PIN\n";
    return 2;
  }
  try {
    moonlight::gamestream::GameStreamClient client(argv[1], 47989);
    client.pair(argv[2]);
    const auto apps = client.apps();
    std::cout << "Paired; received " << apps.size() << " apps\n";
    for (const auto& app : apps) {
      std::cout << app.id << '\t' << app.name << '\n';
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
