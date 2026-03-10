#pragma once

#include <filesystem>

#include "core/report.hpp"

namespace cuda_doctor::commands {

cuda_doctor::core::Report run_doctor(
    bool auto_configure = false,
    const std::filesystem::path& cwd = std::filesystem::current_path());

}
