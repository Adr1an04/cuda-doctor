#include <iostream>
#include <string>

#include "commands/check.hpp"
#include "commands/doctor.hpp"
#include "core/report.hpp"

namespace {

using cuda_doctor::core::Report;

static inline void print_usage() {
  std::cerr << "Usage: cuda-doctor-core <check|doctor> [--json]\n";
}

static inline std::string escape_json(const std::string& text) {
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

static inline void print_text(const std::string& command, const Report& report) {
  std::cout << "cuda-doctor " << command << "\n";
  std::cout << "overall: " << cuda_doctor::core::to_string(report.overall)
            << "\n";
  std::cout << "os: " << report.os << "\n";
  std::cout << "arch: " << report.arch << "\n\n";

  for (const auto& probe : report.probes) {
    std::cout << probe.name << " [" << cuda_doctor::core::to_string(probe.status)
              << "]\n";
    std::cout << "  " << probe.message << "\n";
  }

  if (!report.next_steps.empty()) {
    std::cout << "\nnext steps\n";
    for (const auto& step : report.next_steps) {
      std::cout << "  - " << step << "\n";
    }
  }
}

static inline void print_json(const std::string& command, const Report& report) {
  std::cout << "{\n";
  std::cout << "  \"command\": \"" << escape_json(command) << "\",\n";
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

  std::cout << "  ],\n";
  std::cout << "  \"next_steps\": [\n";
  for (std::size_t i = 0; i < report.next_steps.size(); ++i) {
    std::cout << "    \"" << escape_json(report.next_steps[i]) << "\"";
    if (i + 1 < report.next_steps.size()) {
      std::cout << ",";
    }
    std::cout << "\n";
  }
  std::cout << "  ]\n";
  std::cout << "}\n";
}

static inline bool is_json_output(int argc, char** argv) {
  return argc > 2 && std::string(argv[2]) == "--json";
}

static inline Report run_command(const std::string& command) {
  return command == "doctor" ? cuda_doctor::commands::run_doctor()
                             : cuda_doctor::commands::run_check();
}

}

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage();
    return 2;
  }

  const std::string command = argv[1];
  if (command != "check" && command != "doctor") {
    print_usage();
    return 2;
  }

  const auto report = run_command(command);

  if (is_json_output(argc, argv)) {
    print_json(command, report);
  } else {
    print_text(command, report);
  }

  return report.overall == cuda_doctor::core::Status::kOk ? 0 : 1;
}
