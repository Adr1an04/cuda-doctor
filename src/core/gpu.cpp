#include "core/gpu.hpp"

#include "core/process.hpp"

namespace cuda_doctor::core::gpu {

Probe detect() {
  // Treat `nvidia-smi` as a probe prerequisite, not as proof that no GPU exists.
  if (!process::command_exists("nvidia-smi")) {
    return {
        .name = "gpu",
        .status = Status::kMissing,
        .message = "GPU probe skipped because nvidia-smi was not found.",
    };
  }

  const auto gpu_name = process::capture(
      "nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null");
  return {
      .name = "gpu",
      .status = gpu_name.empty() ? Status::kMissing : Status::kOk,
      .message = gpu_name.empty() ? "No NVIDIA GPU was reported by nvidia-smi."
                                  : "Detected GPU " + gpu_name + ".",
  };
}

}
