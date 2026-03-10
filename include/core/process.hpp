#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace cuda_doctor::core::process {

bool command_exists(const std::string& name);

std::optional<std::filesystem::path> find_command(const std::string& name);

std::string capture(const char* command);

}
