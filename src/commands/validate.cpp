#include "commands/validate.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "commands/check.hpp"
#include "core/cuda_env.hpp"
#include "core/process.hpp"

namespace cuda_doctor::commands {

namespace {

using core::Probe;
using core::Report;
using core::Status;

struct TempWorkspace {
  std::filesystem::path root;

  ~TempWorkspace() {
    if (root.empty()) {
      return;
    }

    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
};

static inline const Probe* find_probe(
    const Report& report,
    std::string_view name) {
  for (const auto& probe : report.probes) {
    if (probe.name == name) {
      return &probe;
    }
  }

  return nullptr;
}

static inline Status summarize_status(const std::vector<Probe>& probes) {
  bool any_issue = false;
  bool any_missing = false;

  for (const auto& probe : probes) {
    if (probe.status == Status::kUnsupported) {
      return Status::kUnsupported;
    }

    if (probe.status == Status::kIssue) {
      any_issue = true;
    }

    if (probe.status == Status::kMissing) {
      any_missing = true;
    }
  }

  if (any_issue) {
    return Status::kIssue;
  }

  return any_missing ? Status::kMissing : Status::kOk;
}

static inline bool is_ok_probe(const Report& report, std::string_view name) {
  const auto* probe = find_probe(report, name);
  return probe != nullptr && probe->status == Status::kOk;
}

static inline std::string quote_path(const std::filesystem::path& path) {
  std::string quoted = "\"";
  for (const char ch : path.string()) {
    if (ch == '"') {
      quoted += "\\\"";
    } else {
      quoted += ch;
    }
  }
  quoted += "\"";
  return quoted;
}

static inline std::string summarize_output(const std::string& output) {
  if (output.empty()) {
    return "no diagnostic output";
  }

  constexpr std::size_t kMaxLength = 220;
  if (output.size() <= kMaxLength) {
    return output;
  }

  return output.substr(0, kMaxLength - 3) + "...";
}

static inline std::filesystem::path make_workspace_root(std::error_code& error) {
  const auto temp_root = std::filesystem::temp_directory_path(error);
  if (error) {
    return {};
  }

  const auto stamp =
      std::chrono::steady_clock::now().time_since_epoch().count();
  return temp_root / ("cuda-doctor-validate-" + std::to_string(stamp));
}

static inline bool write_file(
    const std::filesystem::path& path,
    const std::string& contents) {
  std::ofstream output(path);
  if (!output) {
    return false;
  }

  output << contents;
  return static_cast<bool>(output);
}

static inline std::string smoke_source() {
  return R"(#include <cmath>
#include <iostream>

#include <cuda_runtime.h>

__global__ void add_one(float* values) {
  const int index = blockIdx.x * blockDim.x + threadIdx.x;
  if (index < 64) {
    values[index] += 1.0f;
  }
}

static int check(cudaError_t status, const char* step) {
  if (status == cudaSuccess) {
    return 0;
  }

  std::cerr << step << ": " << cudaGetErrorString(status) << "\n";
  return 1;
}

int main() {
  int device_count = 0;
  if (check(cudaGetDeviceCount(&device_count), "cudaGetDeviceCount") != 0) {
    return 1;
  }

  if (device_count < 1) {
    std::cerr << "No CUDA devices were reported by the runtime.\n";
    return 1;
  }

  if (check(cudaSetDevice(0), "cudaSetDevice") != 0) {
    return 1;
  }

  constexpr int kCount = 64;
  float host[kCount];
  for (int index = 0; index < kCount; ++index) {
    host[index] = static_cast<float>(index);
  }

  float* device = nullptr;
  if (check(
          cudaMalloc(reinterpret_cast<void**>(&device), sizeof(float) * kCount),
          "cudaMalloc") != 0) {
    return 1;
  }

  if (check(
          cudaMemcpy(device, host, sizeof(float) * kCount, cudaMemcpyHostToDevice),
          "cudaMemcpy host->device") != 0) {
    cudaFree(device);
    return 1;
  }

  add_one<<<1, kCount>>>(device);
  if (check(cudaGetLastError(), "kernel launch") != 0) {
    cudaFree(device);
    return 1;
  }

  if (check(cudaDeviceSynchronize(), "cudaDeviceSynchronize") != 0) {
    cudaFree(device);
    return 1;
  }

  if (check(
          cudaMemcpy(host, device, sizeof(float) * kCount, cudaMemcpyDeviceToHost),
          "cudaMemcpy device->host") != 0) {
    cudaFree(device);
    return 1;
  }

  if (check(cudaFree(device), "cudaFree") != 0) {
    return 1;
  }

  for (int index = 0; index < kCount; ++index) {
    const float expected = static_cast<float>(index + 1);
    if (std::fabs(host[index] - expected) > 1e-5f) {
      std::cerr << "Validation mismatch at index " << index << ".\n";
      return 1;
    }
  }

  std::cout << "Validation succeeded on " << device_count
            << " CUDA device(s).";
  return 0;
}
)";
}

static inline Probe run_validation_probe() {
  const auto nvcc = cuda_doctor::core::cuda_env::locate_nvcc();
  if (!nvcc.has_value()) {
    return {
        .name = "validation",
        .status = Status::kMissing,
        .message = "Validation requires nvcc so a CUDA smoke test can be compiled.",
    };
  }

  std::error_code error;
  TempWorkspace workspace{.root = make_workspace_root(error)};
  if (error) {
    return {
        .name = "validation",
        .status = Status::kIssue,
        .message = "Failed to locate a temporary directory for the validation workspace.",
    };
  }

  std::filesystem::create_directories(workspace.root, error);
  if (error) {
    return {
        .name = "validation",
        .status = Status::kIssue,
        .message = "Failed to create a temporary validation workspace at " +
            workspace.root.string() + ".",
    };
  }

  const auto source = workspace.root / "validate_smoke.cu";
#if defined(_WIN32)
  const auto binary = workspace.root / "validate_smoke.exe";
#else
  const auto binary = workspace.root / "validate_smoke";
#endif

  if (!write_file(source, smoke_source())) {
    return {
        .name = "validation",
        .status = Status::kIssue,
        .message = "Failed to write the temporary CUDA smoke test source.",
    };
  }

  const auto compile = cuda_doctor::core::process::run(
      quote_path(*nvcc) + " -std=c++17 " + quote_path(source) + " -o " +
      quote_path(binary) + " 2>&1");
  if (compile.exit_code != 0) {
    return {
        .name = "validation",
        .status = Status::kIssue,
        .message = "Smoke test compilation failed: " + summarize_output(compile.output),
    };
  }

  const auto run = cuda_doctor::core::process::run(quote_path(binary) + " 2>&1");
  if (run.exit_code != 0) {
    return {
        .name = "validation",
        .status = Status::kIssue,
        .message = "Smoke test execution failed: " + summarize_output(run.output),
    };
  }

  return {
      .name = "validation",
      .status = Status::kOk,
      .message = run.output.empty()
                     ? "Smoke test compiled and executed successfully."
                     : summarize_output(run.output),
  };
}

}

Report run_validate() {
  auto report = run_check();

  if (!is_ok_probe(report, "platform")) {
    report.probes.push_back({
        .name = "validation",
        .status = Status::kUnsupported,
        .message = "Validation requires a CUDA-capable host platform.",
    });
    report.next_steps.push_back(
        "Run validation on Linux or Windows with an NVIDIA GPU.");
  } else if (!is_ok_probe(report, "driver") ||
             !is_ok_probe(report, "cuda") ||
             !is_ok_probe(report, "gpu")) {
    report.probes.push_back({
        .name = "validation",
        .status = Status::kMissing,
        .message = "Validation was skipped because driver, toolkit, or GPU probes did not pass.",
    });
    report.next_steps.push_back(
        "Run `cuda-doctor doctor` first and fix the missing driver/toolkit/GPU prerequisites before validation.");
  } else {
    const auto validation = run_validation_probe();
    report.probes.push_back(validation);
    if (validation.status != Status::kOk) {
      report.next_steps.push_back(
          "Fix the compile or runtime error above, then re-run `cuda-doctor validate`.");
    }
  }

  report.overall = summarize_status(report.probes);
  return report;
}

}
