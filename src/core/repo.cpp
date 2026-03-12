#include "core/repo.hpp"

#include <fstream>
#include <set>
#include <string>
#include <system_error>
#include <vector>

namespace cuda_doctor::core::repo {

namespace {

struct PatchRule {
  std::string needle;
  std::string replacement;
  std::string description;
};

static inline const std::vector<PatchRule>& patch_rules() {
  static const auto rules = std::vector<PatchRule>{
      {
          .needle = "-m32",
          .replacement = "",
          .description = "removes the `-m32` flag that forces an unsupported 32-bit CUDA build.",
      },
      {
          .needle = "/MACHINE:X86",
          .replacement = "/MACHINE:X64",
          .description = "switches the linker target from x86 to x64.",
      },
      {
          .needle = "-A Win32",
          .replacement = "-A x64",
          .description = "switches the generator platform from Win32 to x64.",
      },
      {
          .needle = "CMAKE_GENERATOR_PLATFORM Win32",
          .replacement = "CMAKE_GENERATOR_PLATFORM x64",
          .description = "switches CMake generator platform from Win32 to x64.",
      },
      {
          .needle = "CMAKE_GENERATOR_PLATFORM=Win32",
          .replacement = "CMAKE_GENERATOR_PLATFORM=x64",
          .description = "switches CMake generator platform from Win32 to x64.",
      },
      {
          .needle = "CMAKE_VS_PLATFORM_NAME Win32",
          .replacement = "CMAKE_VS_PLATFORM_NAME x64",
          .description = "switches the Visual Studio platform name from Win32 to x64.",
      },
      {
          .needle = "CUDA_32_BIT_DEVICE_CODE ON",
          .replacement = "CUDA_32_BIT_DEVICE_CODE OFF",
          .description = "disables 32-bit CUDA device code generation.",
      },
      {
          .needle = "CUDA_32_BIT_DEVICE_CODE=ON",
          .replacement = "CUDA_32_BIT_DEVICE_CODE=OFF",
          .description = "disables 32-bit CUDA device code generation.",
      },
  };

  return rules;
}

static inline bool should_skip_directory(const std::filesystem::path& path) {
  const auto name = path.filename().string();
  return name == ".git" || name == ".dist" || name == "build" ||
         name == "node_modules" || name == "third_party";
}

static inline bool is_manifest_file(const std::filesystem::path& path) {
  const auto name = path.filename().string();
  return name == "CMakeLists.txt" || name == "package.json" || name == "Makefile" ||
         name == "GNUmakefile" || path.extension() == ".cmake" || path.extension() == ".mk";
}

static inline std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path);
  return std::string(
      std::istreambuf_iterator<char>(input),
      std::istreambuf_iterator<char>());
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

static inline std::string make_relative(
    const std::filesystem::path& cwd,
    const std::filesystem::path& path) {
  std::error_code error;
  const auto relative = std::filesystem::relative(path, cwd, error);
  return error ? path.string() : relative.string();
}

static inline std::vector<std::filesystem::path> collect_manifest_files(
    const std::filesystem::path& cwd) {
  std::vector<std::filesystem::path> files;

  if (!std::filesystem::exists(cwd) || !std::filesystem::is_directory(cwd)) {
    return files;
  }

  std::error_code error;
  for (std::filesystem::recursive_directory_iterator it(cwd, error), end;
       it != end && !error;
       it.increment(error)) {
    if (it->is_directory()) {
      if (should_skip_directory(it->path())) {
        it.disable_recursion_pending();
      }
      continue;
    }

    if (is_manifest_file(it->path())) {
      files.push_back(it->path());
    }
  }

  return files;
}

static inline std::vector<Issue> scan_file(
    const std::filesystem::path& cwd,
    const std::filesystem::path& path) {
  std::vector<Issue> issues;
  std::ifstream input(path);
  std::string line;
  std::size_t line_number = 0;

  while (std::getline(input, line)) {
    ++line_number;

    for (const auto& rule : patch_rules()) {
      if (line.find(rule.needle) == std::string::npos) {
        continue;
      }

      issues.push_back({
          .file = std::filesystem::path(make_relative(cwd, path)),
          .line = line_number,
          .description = rule.description,
          .snippet = line,
      });
    }
  }

  return issues;
}

static inline void replace_all(
    std::string& text,
    const std::string& needle,
    const std::string& replacement) {
  std::size_t position = 0;
  while ((position = text.find(needle, position)) != std::string::npos) {
    text.replace(position, needle.size(), replacement);
    position += replacement.size();
  }
}

}

ScanResult scan(const std::filesystem::path& cwd) {
  const auto manifest_files = collect_manifest_files(cwd);
  std::vector<Issue> issues;

  for (const auto& path : manifest_files) {
    const auto file_issues = scan_file(cwd, path);
    issues.insert(issues.end(), file_issues.begin(), file_issues.end());
  }

  if (manifest_files.empty()) {
    return {
        .probe =
            {
                .name = "repo",
                .status = Status::kOk,
                .message = "No supported repo build manifests were found in this directory.",
            },
        .issues = {},
    };
  }

  if (issues.empty()) {
    return {
        .probe =
            {
                .name = "repo",
                .status = Status::kOk,
                .message = "No repo-level 32-bit CUDA compatibility issues were detected in build manifests.",
            },
        .issues = {},
    };
  }

  return {
      .probe =
          {
              .name = "repo",
              .status = Status::kIssue,
              .message = "Detected " + std::to_string(issues.size()) +
                  " repo-level CUDA compatibility issue(s) tied to 32-bit build targets.",
          },
      .issues = std::move(issues),
  };
}

AutoFixResult apply_fixes(const std::filesystem::path& cwd) {
  const auto scan_result = scan(cwd);
  if (scan_result.issues.empty()) {
    return {
        .probe =
            {
                .name = "repo",
                .status = Status::kOk,
                .message = "Repo build manifests already avoid known 32-bit CUDA targets.",
            },
        .changed_files = {},
    };
  }

  std::set<std::filesystem::path> changed_files;

  for (const auto& issue : scan_result.issues) {
    const auto file = cwd / issue.file;
    auto contents = read_file(file);
    const auto original = contents;

    for (const auto& rule : patch_rules()) {
      replace_all(contents, rule.needle, rule.replacement);
    }

    if (contents != original && write_file(file, contents)) {
      changed_files.insert(issue.file);
    }
  }

  const auto rescanned = scan(cwd);
  if (rescanned.issues.empty()) {
    return {
        .probe =
            {
                .name = "repo",
                .status = Status::kOk,
                .message = changed_files.empty()
                               ? "Repo build manifests are already compatible with modern CUDA targets."
                               : "Patched " + std::to_string(scan_result.issues.size()) +
                                   " repo-level CUDA compatibility issue(s) across " +
                                   std::to_string(changed_files.size()) + " file(s).",
            },
        .changed_files = std::vector<std::filesystem::path>(changed_files.begin(), changed_files.end()),
    };
  }

  return {
      .probe =
          {
              .name = "repo",
              .status = Status::kIssue,
              .message = changed_files.empty()
                             ? "Detected repo-level CUDA compatibility issues, but no automatic patch was applied."
                             : "Patched " + std::to_string(changed_files.size()) +
                                 " file(s), but " + std::to_string(rescanned.issues.size()) +
                                 " repo-level issue(s) still remain.",
          },
      .changed_files = std::vector<std::filesystem::path>(changed_files.begin(), changed_files.end()),
  };
}

}
