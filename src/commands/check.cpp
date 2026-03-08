#include "commands/check.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>

namespace cuda_doctor::commands {

namespace {

std::string trim(std::string text) {
  while (!text.empty() &&
         (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) {
    text.pop_back();
  }

  std::size_t first = 0;
  while (first < text.size() && text[first] == ' ') {
    ++first;
  }

  return text.substr(first);
}

std::string detect_os() {
#if defined(__APPLE__)
  return "macos";
#elif defined(__linux__)
  return "linux";
#elif defined(_WIN32)
  return "windows";
#else
  return "unknown";
#endif
}

std::string detect_arch() {
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
  return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
  return "x86_64";
#else
  return "unknown";
#endif
}

bool command_exists(const std::string& name) {
  const char* path_env = std::getenv("PATH");
  if (path_env == nullptr) {
    return false;
  }

  std::stringstream path_stream(path_env);
  std::string entry;
  while (std::getline(path_stream, entry, ':')) {
    if (entry.empty()) {
      continue;
    }

    const auto candidate = std::filesystem::path(entry) / name;
    if (std::filesystem::exists(candidate)) {
      return true;
    }
  }

  return false;
}

std::string capture(const char* command) {
  std::string output;
  FILE* pipe = popen(command, "r");
  if (pipe == nullptr) {
    return output;
  }

  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
  }

  pclose(pipe);
  return trim(output);
}

cuda_doctor::core::Status summarize(bool unsupported, bool driver_ok, bool cuda_ok) {
  if (unsupported) {
    return cuda_doctor::core::Status::kUnsupported;
  }

  return (driver_ok && cuda_ok) ? cuda_doctor::core::Status::kOk
                                : cuda_doctor::core::Status::kMissing;
}

}  // namespace

cuda_doctor::core::Report run_check() {
  cuda_doctor::core::Report report;
  report.os = detect_os();
  report.arch = detect_arch();

  const bool unsupported = report.os == "macos";
  const bool has_driver = command_exists("nvidia-smi");
  const bool has_nvcc = command_exists("nvcc");

  report.probes.push_back({
      .name = "platform",
      .status = unsupported ? cuda_doctor::core::Status::kUnsupported
                            : cuda_doctor::core::Status::kOk,
      .message = unsupported
                     ? "NVIDIA CUDA is not supported on this macOS host."
                     : "Host platform can support an NVIDIA CUDA stack.",
  });

  report.probes.push_back({
      .name = "driver",
      .status = has_driver ? cuda_doctor::core::Status::kOk
                           : cuda_doctor::core::Status::kMissing,
      .message = has_driver
                     ? "Found nvidia-smi: " +
                           capture("nvidia-smi --query-gpu=driver_version --format=csv,noheader 2>/dev/null")
                     : "nvidia-smi was not found.",
  });

  report.probes.push_back({
      .name = "cuda",
      .status = has_nvcc ? cuda_doctor::core::Status::kOk
                         : cuda_doctor::core::Status::kMissing,
      .message = has_nvcc ? "Found nvcc: " + capture("nvcc --version 2>/dev/null")
                          : "nvcc was not found.",
  });

  report.overall = summarize(unsupported, has_driver, has_nvcc);
  return report;
}

}  // namespace cuda_doctor::commands
