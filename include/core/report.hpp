#pragma once

#include <string>
#include <vector>

namespace cuda_doctor::core {

enum class Status {
  kOk,
  kMissing,
  kUnsupported,
};

struct Probe {
  std::string name;
  Status status;
  std::string message;
};

struct Report {
  std::string os;
  std::string arch;
  Status overall;
  std::vector<Probe> probes;
  std::vector<std::string> next_steps;
};

inline const char* to_string(Status status) noexcept {
  switch (status) {
    case Status::kOk:
      return "ok";
    case Status::kMissing:
      return "missing";
    case Status::kUnsupported:
      return "unsupported";
  }

  return "missing";
}

}
