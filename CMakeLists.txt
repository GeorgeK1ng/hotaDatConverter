cmake_minimum_required(VERSION 3.20)
project(hotaDatConverter LANGUAGES CXX)

# C++17, but old Win API (XP)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options so you can tweak from command line if needed
option(STATIC_LINK "Link statically (libstdc++/libgcc)" ON)
option(OPT_SIZE "Optimize for size" ON)

# Flags – final values can be overridden by toolchain files
if (MINGW)
  add_compile_definitions(NOMINMAX)

  if (STATIC_LINK)
    set(_STATIC_FLAGS "-static -static-libstdc++ -static-libgcc")
  endif()

  if (OPT_SIZE)
    add_compile_options(-Os)
  endif()

  # These vars can be prefilled by toolchain. Only append here.
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${_STATIC_FLAGS}")
endif()

# Bring in nlohmann/json (header-only)
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(hotaDatConverter
  hotaDatConverter.cpp
)

target_link_libraries(hotaDatConverter PRIVATE nlohmann_json::nlohmann_json)

# MinGW: strip symbols in Release to keep exe tiny
if (MINGW)
  if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET hotaDatConverter POST_BUILD
      COMMAND ${CMAKE_STRIP} $<TARGET_FILE:hotaDatConverter>
      VERBATIM)
  endif()
endif()
