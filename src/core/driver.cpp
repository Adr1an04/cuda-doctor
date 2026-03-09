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

  const auto version = process::capture(
      "nvidia-smi --query-gpu=driver_version --format=csv,noheader 2>/dev/null");
  return {
      .name = "driver",
      .status = Status::kOk,
      .message = version.empty()
                     ? "Found nvidia-smi, but the driver version query returned no output."
                     : "Detected NVIDIA driver version " + version + ".",
  };
}

}
