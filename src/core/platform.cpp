#include "core/platform.hpp"

namespace cuda_doctor::core::platform {

namespace {

static inline std::string detect_os() {
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

static inline std::string detect_arch() {
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
  return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
  return "x86_64";
#else
  return "unknown";
#endif
}

static inline bool is_unsupported_os(const std::string& os) {
  return os == "macos";
}

}

PlatformInfo detect() {
  const auto os = detect_os();
  const auto arch = detect_arch();
  const bool unsupported = is_unsupported_os(os);

  return {
      .os = os,
      .arch = arch,
      .unsupported = unsupported,
      .probe =
          {
              .name = "platform",
              .status = unsupported ? Status::kUnsupported : Status::kOk,
              .message = unsupported
                             ? "NVIDIA CUDA is not supported on this macOS host."
                             : "Host platform can support an NVIDIA CUDA stack.",
          },
  };
}

}
