cmake_minimum_required(VERSION 3.20)

project(cuda_doctor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(
  cuda-doctor-core
  src/main.cpp
  src/commands/check.cpp
  src/commands/doctor.cpp
  src/core/configure.cpp
  src/core/process.cpp
  src/core/platform.cpp
  src/core/driver.cpp
  src/core/cuda_env.cpp
  src/core/gpu.cpp
)

target_include_directories(cuda-doctor-core PRIVATE include)
