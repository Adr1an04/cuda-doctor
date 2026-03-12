#include "commands/check.hpp"

#include <utility>

#include "core/cuda_env.hpp"
#include "core/driver.hpp"
#include "core/gpu.hpp"
#include "core/platform.hpp"

namespace cuda_doctor::commands {

namespace {

using core::Probe;
using core::Report;
using core::Status;

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

}

Report run_check() {
  const auto platform = cuda_doctor::core::platform::detect();
  auto probes = std::vector<Probe>{
      platform.probe,
      cuda_doctor::core::driver::detect(),
      cuda_doctor::core::cuda_env::detect(),
      cuda_doctor::core::gpu::detect(),
  };

  return {
      .os = platform.os,
      .arch = platform.arch,
      .overall = summarize_status(probes),
      .probes = std::move(probes),
      .next_steps = {},
  };
}

}
