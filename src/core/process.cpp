#include "core/process.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace cuda_doctor::core::process {

namespace {

static inline bool is_command_path(const std::filesystem::path& path) {
  return std::filesystem::exists(path) && !std::filesystem::is_directory(path);
}

static inline std::string trim(std::string text) {
  while (!text.empty() &&
         (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) {
    text.pop_back();
  }

  std::size_t first = 0;
  while (first < text.size() && text[first] == ' ') {
    ++first;
  }

  return text.substr(first);
}

static inline int decode_exit_code(const int status) {
  if (status == -1) {
    return -1;
  }

#if defined(_WIN32)
  return status;
#else
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }

  if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }

  return status;
#endif
}

}

std::optional<std::filesystem::path> find_command(const std::string& name) {
  const char* path_env = std::getenv("PATH");
  if (path_env == nullptr) {
    return std::nullopt;
  }

  std::stringstream path_stream(path_env);
  std::string entry;
  while (std::getline(path_stream, entry, ':')) {
    if (entry.empty()) {
      continue;
    }

    const auto candidate = std::filesystem::path(entry) / name;
    if (is_command_path(candidate)) {
      return candidate;
    }
  }

  return std::nullopt;
}

bool command_exists(const std::string& name) {
  return find_command(name).has_value();
}

CommandResult run(const std::string& command) {
  std::string output;
  FILE* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    return {
        .exit_code = -1,
        .output = {},
    };
  }

  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
  }

  return {
      .exit_code = decode_exit_code(pclose(pipe)),
      .output = trim(output),
  };
}

std::string capture(const char* command) {
  return run(command).output;
}

}
