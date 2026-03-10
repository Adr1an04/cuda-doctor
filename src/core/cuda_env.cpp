#include "core/cuda_env.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "core/process.hpp"

namespace cuda_doctor::core::cuda_env {

namespace {

static inline bool is_nvcc(const std::filesystem::path& path) {
  return std::filesystem::exists(path) && !std::filesystem::is_directory(path);
}

static inline std::vector<std::filesystem::path> candidate_roots() {
  std::vector<std::filesystem::path> roots;

  if (const char* cuda_home = std::getenv("CUDA_HOME")) {
    roots.emplace_back(cuda_home);
  }

  if (const char* cuda_path = std::getenv("CUDA_PATH")) {
    roots.emplace_back(cuda_path);
  }

  roots.emplace_back("/usr/local/cuda");
  roots.emplace_back("/opt/cuda");

  for (const auto& parent : {std::filesystem::path("/usr/local"),
                             std::filesystem::path("/opt")}) {
    if (!std::filesystem::exists(parent) || !std::filesystem::is_directory(parent)) {
      continue;
    }

    for (const auto& entry : std::filesystem::directory_iterator(parent)) {
      if (!entry.is_directory()) {
        continue;
      }

      const auto name = entry.path().filename().string();
      if (name == "cuda" || name.rfind("cuda-", 0) == 0) {
        roots.push_back(entry.path());
      }
    }
  }

  return roots;
}

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

std::optional<std::filesystem::path> locate_nvcc() {
  if (const auto nvcc = process::find_command("nvcc")) {
    return nvcc;
  }

  for (const auto& root : candidate_roots()) {
    const auto candidate = root / "bin" / "nvcc";
    if (is_nvcc(candidate)) {
      return candidate;
    }
  }

  return std::nullopt;
}

std::optional<std::filesystem::path> locate_toolkit_root() {
  const auto nvcc = locate_nvcc();
  if (!nvcc.has_value()) {
    return std::nullopt;
  }

  const auto bin_dir = nvcc->parent_path();
  if (bin_dir.filename() != "bin") {
    return std::nullopt;
  }

  return bin_dir.parent_path();
}

Probe detect() {
  const auto nvcc = locate_nvcc();
  if (!nvcc.has_value()) {
    return {
        .name = "cuda",
        .status = Status::kMissing,
        .message = "nvcc was not found.",
    };
  }

  const auto command = nvcc->string() + " --version 2>/dev/null";
  const auto release = parse_release(process::capture(command.c_str()));
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
