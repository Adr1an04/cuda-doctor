#pragma once

#include <filesystem>

#include "core/report.hpp"

namespace cuda_doctor::core::configure {

struct AutoConfigureResult {
  Probe probe;
  std::filesystem::path env_file;
  bool changed;
};

AutoConfigureResult apply_cuda_env(
    const std::string& os,
    const std::filesystem::path& cwd);

}
