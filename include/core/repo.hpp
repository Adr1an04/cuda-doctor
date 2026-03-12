#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/report.hpp"

namespace cuda_doctor::core::repo {

struct Issue {
  std::filesystem::path file;
  std::size_t line;
  std::string description;
  std::string snippet;
};

struct ScanResult {
  Probe probe;
  std::vector<Issue> issues;
};

struct AutoFixResult {
  Probe probe;
  std::vector<std::filesystem::path> changed_files;
};

ScanResult scan(const std::filesystem::path& cwd);

AutoFixResult apply_fixes(const std::filesystem::path& cwd);

}
