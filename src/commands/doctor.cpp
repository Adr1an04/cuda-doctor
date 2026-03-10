#include "commands/doctor.hpp"

#include <string>
#include <string_view>

#include "commands/check.hpp"
#include "core/configure.hpp"

namespace cuda_doctor::commands {

namespace {

using core::Probe;
using core::Report;
using core::Status;

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

static inline void append_step_if_missing(
    Report& report,
    std::string_view probe_name,
    std::string step) {
  const auto* probe = find_probe(report, probe_name);
  if (probe != nullptr && probe->status == Status::kMissing) {
    report.next_steps.push_back(std::move(step));
  }
}

}

Report run_doctor(bool auto_configure, const std::filesystem::path& cwd) {
  auto report = run_check();

  if (auto_configure) {
    const auto auto_result = cuda_doctor::core::configure::apply_cuda_env(report.os, cwd);
    report.probes.push_back(auto_result.probe);
    if (auto_result.probe.status == Status::kOk) {
      report.next_steps.push_back(
          "Run `source " + auto_result.env_file.filename().string() + "` before building in this shell.");
      report.next_steps.push_back(
          "Re-run `cuda-doctor doctor` after sourcing the env file.");
    }
  }

  // On macOS the right answer is "unsupported", not "install more CUDA bits".
  if (report.overall == Status::kUnsupported && report.os == "macos") {
    report.next_steps.push_back(
        "Use Linux, WSL2, or Windows with an NVIDIA GPU for real CUDA workloads.");
    report.next_steps.push_back(
        "Treat this Mac as a development host for the CLI, not as a CUDA runtime target.");
    return report;
  }

  append_step_if_missing(
      report,
      "driver",
      "Install an NVIDIA driver before attempting CUDA toolkit setup.");
  append_step_if_missing(
      report,
      "cuda",
      "Install the CUDA toolkit and make sure `nvcc` is on PATH.");
  append_step_if_missing(
      report,
      "gpu",
      "Verify `nvidia-smi` can enumerate the GPU before trusting the setup.");

  if (report.next_steps.empty()) {
    report.next_steps.push_back(
        "Base checks passed. Next step is adding a real validation command to prove kernel execution.");
  }

  return report;
}

}
