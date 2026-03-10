#pragma once

#include <filesystem>
#include <optional>

#include "core/report.hpp"

namespace cuda_doctor::core::cuda_env {

Probe detect();

std::optional<std::filesystem::path> locate_nvcc();

std::optional<std::filesystem::path> locate_toolkit_root();

}
