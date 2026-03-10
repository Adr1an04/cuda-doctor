#include "core/configure.hpp"

#include <fstream>
#include <string>

#include "core/cuda_env.hpp"

namespace cuda_doctor::core::configure {

namespace {

static inline std::string render_env_file(
    const std::filesystem::path& toolkit_root) {
  const auto lib64_dir = toolkit_root / "lib64";
  const auto lib_dir = std::filesystem::exists(lib64_dir) ? lib64_dir : toolkit_root / "lib";

  return
      "export CUDA_HOME=\"" + toolkit_root.string() + "\"\n"
      "export PATH=\"$CUDA_HOME/bin:$PATH\"\n"
      "export LD_LIBRARY_PATH=\"" + lib_dir.string() + ":${LD_LIBRARY_PATH:-}\"\n";
}

static inline std::string read_file_if_exists(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    return {};
  }

  std::ifstream input(path);
  return std::string(
      std::istreambuf_iterator<char>(input),
      std::istreambuf_iterator<char>());
}

}

AutoConfigureResult apply_cuda_env(
    const std::string& os,
    const std::filesystem::path& cwd) {
  const auto env_file = cwd / ".cuda-doctor.env";

  if (os == "macos") {
    return {
        .probe =
            {
                .name = "configuration",
                .status = Status::kUnsupported,
                .message = "Automatic CUDA environment configuration is disabled on macOS.",
            },
        .env_file = env_file,
        .changed = false,
    };
  }

  const auto toolkit_root = cuda_doctor::core::cuda_env::locate_toolkit_root();
  if (!toolkit_root.has_value()) {
    return {
        .probe =
            {
                .name = "configuration",
                .status = Status::kMissing,
                .message = "Could not find a CUDA toolkit root to configure automatically.",
            },
        .env_file = env_file,
        .changed = false,
    };
  }

  const auto contents = render_env_file(*toolkit_root);
  const bool changed = read_file_if_exists(env_file) != contents;

  if (changed) {
    std::ofstream output(env_file);
    output << contents;
  }

  return {
      .probe =
          {
              .name = "configuration",
              .status = Status::kOk,
              .message = changed
                             ? "Wrote " + env_file.filename().string() + " for toolkit " + toolkit_root->string() + "."
                             : env_file.filename().string() + " is already up to date.",
          },
      .env_file = env_file,
      .changed = changed,
  };
}

}
