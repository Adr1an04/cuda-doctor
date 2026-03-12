#include "commands/doctor.hpp"

#include <string>
#include <string_view>

#include "commands/check.hpp"
#include "core/configure.hpp"
#include "core/repo.hpp"

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

static inline void append_step_if_missing(
    Report& report,
    std::string_view probe_name,
    std::string step) {
  const auto* probe = find_probe(report, probe_name);
  if (probe != nullptr && probe->status == Status::kMissing) {
    report.next_steps.push_back(std::move(step));
  }
}

static inline void append_step_if_issue(
    Report& report,
    std::string_view probe_name,
    std::string step) {
  const auto* probe = find_probe(report, probe_name);
  if (probe != nullptr && probe->status == Status::kIssue) {
    report.next_steps.push_back(std::move(step));
  }
}

}

Report run_doctor(bool auto_configure, const std::filesystem::path& cwd) {
  auto report = run_check();
  const auto repo_scan = cuda_doctor::core::repo::scan(cwd);
  const bool unsupported_macos =
      report.overall == Status::kUnsupported && report.os == "macos";

  if (auto_configure) {
    const auto env_result =
        cuda_doctor::core::configure::apply_cuda_env(report.os, cwd);
    report.probes.push_back(env_result.probe);
    if (env_result.probe.status == Status::kOk) {
      report.next_steps.push_back(
          "Run `source " + env_result.env_file.filename().string() + "` before building in this shell.");
      report.next_steps.push_back(
          "Re-run `cuda-doctor doctor` after sourcing the env file.");
    }

    const auto repo_fix = cuda_doctor::core::repo::apply_fixes(cwd);
    report.probes.push_back(repo_fix.probe);
    if (!repo_fix.changed_files.empty()) {
      report.next_steps.push_back(
          "Review the patched repo build files before committing the changes.");
    }
  } else {
    report.probes.push_back(repo_scan.probe);
  }

  if (unsupported_macos) {
    report.next_steps.push_back(
        "Use Linux, WSL2, or Windows with an NVIDIA GPU for real CUDA workloads.");
    report.next_steps.push_back(
        "Treat this Mac as a development host for the CLI, not as a CUDA runtime target.");
  }

  if (!unsupported_macos) {
    append_step_if_missing(
        report,
        "driver",
        "Install an NVIDIA driver before attempting CUDA toolkit setup.");
    append_step_if_issue(
        report,
        "driver",
        "The NVIDIA driver toolchain is present but unhealthy. Reinstall or repair the driver query path.");
    append_step_if_missing(
        report,
        "cuda",
        "Install the CUDA toolkit and make sure `nvcc` is on PATH.");
    append_step_if_issue(
        report,
        "cuda",
        "The CUDA toolkit is present but unhealthy. Verify `nvcc --version` and the selected toolkit root.");
    append_step_if_missing(
        report,
        "gpu",
        "Verify `nvidia-smi` can enumerate the GPU before trusting the setup.");
    append_step_if_issue(
        report,
        "gpu",
        "The GPU probe failed even though the driver tool is present. Re-check the driver/runtime state before building.");
  }

  append_step_if_issue(
      report,
      "repo",
      "This repo still targets unsupported 32-bit CUDA settings. Run `cuda-doctor doctor auto` to patch them.");

  if (report.next_steps.empty()) {
    report.next_steps.push_back(
        "Base checks passed. Next step is adding a real validation command to prove kernel execution.");
  }

  report.overall = summarize_status(report.probes);
  return report;
}

}
