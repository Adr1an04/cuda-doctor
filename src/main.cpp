#include <filesystem>
#include <iostream>
#include <string>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#include "commands/check.hpp"
#include "commands/doctor.hpp"
#include "core/report.hpp"

namespace {

using cuda_doctor::core::Report;

static inline void print_usage() {
  std::cerr << "Usage: cuda-doctor-core <check|doctor> [auto] [--json]\n";
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
  for (int i = 2; i < argc; ++i) {
    if (std::string(argv[i]) == "--json") {
      return true;
    }
  }

  return false;
}

static inline bool is_auto_configure(int argc, char** argv) {
  for (int i = 2; i < argc; ++i) {
    if (std::string(argv[i]) == "auto") {
      return true;
    }
  }

  return false;
}

static inline Report run_command(
    const std::string& command,
    bool auto_configure) {
  return command == "doctor" ? cuda_doctor::commands::run_doctor(auto_configure)
                             : cuda_doctor::commands::run_check();
}

static inline bool has_repo_issue(const Report& report) {
  for (const auto& probe : report.probes) {
    if (probe.name == "repo" && probe.status == cuda_doctor::core::Status::kIssue) {
      return true;
    }
  }

  return false;
}

static inline bool is_interactive_terminal() {
#if defined(_WIN32)
  return _isatty(_fileno(stdin)) && _isatty(_fileno(stdout));
#else
  return isatty(fileno(stdin)) && isatty(fileno(stdout));
#endif
}

static inline bool prompt_for_repo_fix() {
  std::cout << "\nThis repo has dependency issues with 32-bit CUDA targets.\n";
  std::cout << "Would you want to fix it? [y/N] " << std::flush;

  std::string answer;
  if (!std::getline(std::cin, answer)) {
    return false;
  }

  return answer == "y" || answer == "Y" || answer == "yes" || answer == "YES";
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

  const bool auto_configure = is_auto_configure(argc, argv);
  if (command != "doctor" && auto_configure) {
    print_usage();
    return 2;
  }

  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--json") {
      continue;
    }

    if (command == "doctor" && arg == "auto") {
      continue;
    }

    print_usage();
    return 2;
  }

  auto report = run_command(command, auto_configure);
  const std::string display_command =
      command == "doctor" && auto_configure ? "doctor auto" : command;

  if (is_json_output(argc, argv)) {
    print_json(display_command, report);
  } else {
    print_text(display_command, report);
  }

  if (!is_json_output(argc, argv) &&
      command == "doctor" &&
      !auto_configure &&
      has_repo_issue(report) &&
      is_interactive_terminal() &&
      prompt_for_repo_fix()) {
    report = cuda_doctor::commands::run_doctor(true, std::filesystem::current_path());
    std::cout << "\n";
    print_text("doctor auto", report);
  }

  return report.overall == cuda_doctor::core::Status::kOk ? 0 : 1;
}
