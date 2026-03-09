#pragma once

#include <string>

#include "core/report.hpp"

namespace cuda_doctor::core::platform {

struct PlatformInfo {
  std::string os;
  std::string arch;
  bool unsupported;
  Probe probe;
};

PlatformInfo detect();

}
