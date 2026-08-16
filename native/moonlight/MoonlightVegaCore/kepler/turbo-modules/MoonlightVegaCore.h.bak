#pragma once

#include "generated/MoonlightVegaCoreSpec.h"

namespace MoonlightVegaCoreTurboModule {

class MoonlightVegaCore : public MoonlightVegaCoreSpec {
 public:
  MoonlightVegaCore();
  ~MoonlightVegaCore() noexcept override;

  com::amazon::kepler::turbomodule::JSObject getCoreInfo() override;
  com::amazon::kepler::turbomodule::Promise getServerInfo(
      std::string host,
      double port) override;
};

}  // namespace MoonlightVegaCoreTurboModule

