#include <iostream>
#include <string>

#include "commands/check.hpp"
#include "core/report.hpp"

namespace {

void print_usage() {
  std::cerr << "Usage: cuda-doctor-core check [--json]\n";
}

std::string escape_json(const std::string& text) {
  std::string escaped;
  escaped.reserve(text.size());
  for (const char ch : text) {
    if (ch == '\\') {
      escaped += "\\\\";
    } else if (ch == '"') {
      escaped += "\\\"";
    } else if (ch == '\n') {
      escaped += "\\n";
    } else {
      escaped += ch;
    }
  }
  return escaped;
}

void print_text(const cuda_doctor::core::Report& report) {
  std::cout << "cuda-doctor check\n";
  std::cout << "overall: " << cuda_doctor::core::to_string(report.overall)
            << "\n";
  std::cout << "os: " << report.os << "\n";
  std::cout << "arch: " << report.arch << "\n\n";

  for (const auto& probe : report.probes) {
    std::cout << probe.name << " [" << cuda_doctor::core::to_string(probe.status)
              << "]\n";
    std::cout << "  " << probe.message << "\n";
  }
}

void print_json(const cuda_doctor::core::Report& report) {
  std::cout << "{\n";
  std::cout << "  \"overall\": \"" << cuda_doctor::core::to_string(report.overall)
            << "\",\n";
  std::cout << "  \"os\": \"" << escape_json(report.os) << "\",\n";
  std::cout << "  \"arch\": \"" << escape_json(report.arch) << "\",\n";
  std::cout << "  \"probes\": [\n";

  for (std::size_t i = 0; i < report.probes.size(); ++i) {
    const auto& probe = report.probes[i];
    std::cout << "    {\"name\": \"" << escape_json(probe.name)
              << "\", \"status\": \""
              << cuda_doctor::core::to_string(probe.status)
              << "\", \"message\": \"" << escape_json(probe.message) << "\"}";
    if (i + 1 < report.probes.size()) {
      std::cout << ",";
    }
    std::cout << "\n";
  }

  std::cout << "  ]\n";
  std::cout << "}\n";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage();
    return 2;
  }

  const std::string command = argv[1];
  if (command != "check") {
    print_usage();
    return 2;
  }

  const bool json_output = argc > 2 && std::string(argv[2]) == "--json";
  const auto report = cuda_doctor::commands::run_check();

  if (json_output) {
    print_json(report);
  } else {
    print_text(report);
  }

  return report.overall == cuda_doctor::core::Status::kOk ? 0 : 1;
}
