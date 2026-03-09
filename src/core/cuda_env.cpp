#include "core/cuda_env.hpp"

#include <string>

#include "core/process.hpp"

namespace cuda_doctor::core::cuda_env {

namespace {

static inline std::string parse_release(const std::string& nvcc_output) {
  const std::string marker = "release ";
  const auto marker_pos = nvcc_output.find(marker);
  if (marker_pos == std::string::npos) {
    return {};
  }

  const auto start = marker_pos + marker.size();
  const auto end = nvcc_output.find_first_of(",\n", start);
  return nvcc_output.substr(start, end - start);
}

}

Probe detect() {
  const bool has_nvcc = process::command_exists("nvcc");
  if (!has_nvcc) {
    return {
        .name = "cuda",
        .status = Status::kMissing,
        .message = "nvcc was not found.",
    };
  }

  const auto release = parse_release(process::capture("nvcc --version 2>/dev/null"));
  return {
      .name = "cuda",
      .status = Status::kOk,
      // `nvcc` exists, but a missing release string usually means the local toolchain is odd
      .message = release.empty()
                     ? "Detected nvcc, but the CUDA release could not be parsed."
                     : "Detected CUDA toolkit release " + release + ".",
  };
}

}
