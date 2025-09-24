cmake_minimum_required(VERSION 3.20)
project(hotaDatConverter LANGUAGES CXX)

# Use C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prefer static runtime on MSVC so the EXE is self-contained (no VC redist required).
if (MSVC)
  foreach(flag_var
      CMAKE_C_FLAGS
      CMAKE_C_FLAGS_DEBUG
      CMAKE_C_FLAGS_RELEASE
      CMAKE_C_FLAGS_MINSIZEREL
      CMAKE_C_FLAGS_RELWITHDEBINFO
      CMAKE_CXX_FLAGS
      CMAKE_CXX_FLAGS_DEBUG
      CMAKE_CXX_FLAGS_RELEASE
      CMAKE_CXX_FLAGS_MINSIZEREL
      CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    if(${flag_var} MATCHES "/MD")
      string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    endif()
  endforeach()
endif()

# Fetch nlohmann/json header-only library
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Build the executable
add_executable(hotaDatConverter hotaDatConverter.cpp)
target_link_libraries(hotaDatConverter PRIVATE nlohmann_json::nlohmann_json)

# Define NOMINMAX on MSVC to avoid macro conflicts with std::min/std::max
if (WIN32 AND MSVC)
  target_compile_definitions(hotaDatConverter PRIVATE NOMINMAX)
endif()
