#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace cuda_doctor::core::process {

struct CommandResult {
  int exit_code;
  std::string output;
};

bool command_exists(const std::string& name);

std::optional<std::filesystem::path> find_command(const std::string& name);

CommandResult run(const std::string& command);

std::string capture(const char* command);

}
