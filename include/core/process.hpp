#pragma once

#include <string>

namespace cuda_doctor::core::process {

bool command_exists(const std::string& name);

std::string capture(const char* command);

}
