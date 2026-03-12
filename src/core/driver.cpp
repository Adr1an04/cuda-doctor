#include "core/driver.hpp"

#include "core/process.hpp"

namespace cuda_doctor::core::driver {

Probe detect() {
  const bool has_driver = process::command_exists("nvidia-smi");
  if (!has_driver) {
    return {
        .name = "driver",
        .status = Status::kMissing,
        .message = "nvidia-smi was not found.",
    };
  }

  const auto result = process::run(
      "nvidia-smi --query-gpu=driver_version --format=csv,noheader 2>/dev/null");
  return {
      .name = "driver",
      .status = result.exit_code == 0 && !result.output.empty() ? Status::kOk
                                                                : Status::kIssue,
      .message = result.exit_code != 0
                     ? "Found nvidia-smi, but the driver version query failed."
                 : result.output.empty()
                     ? "Found nvidia-smi, but the driver version query returned no output."
                     : "Detected NVIDIA driver version " + result.output + ".",
  };
}

}
