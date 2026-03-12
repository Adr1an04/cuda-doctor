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

  const auto result = process::run(
      "nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null");
  return {
      .name = "gpu",
      .status = result.exit_code == 0 && !result.output.empty() ? Status::kOk
                                                                : Status::kIssue,
      .message = result.exit_code != 0
                     ? "nvidia-smi is present, but the GPU query failed."
                 : result.output.empty()
                     ? "nvidia-smi ran, but no NVIDIA GPU was reported."
                     : "Detected GPU " + result.output + ".",
  };
}

}
