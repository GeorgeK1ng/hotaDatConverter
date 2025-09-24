# MinGW-w64 32-bit toolchain targeting Windows XP (NT 5.1), static link
set(CMAKE_SYSTEM_NAME Windows)

set(CMAKE_C_COMPILER   i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)

set(CMAKE_GENERATOR "Ninja" CACHE STRING "")

# Target XP 32-bit: WINVER/NT 0x0501 (XP)
set(XP_DEFS "-DWINVER=0x0501 -D_WIN32_WINNT=0x0501")
set(STATIC_FLAGS "-static -static-libgcc -static-libstdc++")

# Compile flags
set(CMAKE_C_FLAGS_INIT   "${XP_DEFS} -Os")
set(CMAKE_CXX_FLAGS_INIT "${XP_DEFS} -Os")

# Linker flags:
#  - static link
#  - subsystem console
#  - subsystem version 5.01 (set via *major/minor* options)
set(CMAKE_EXE_LINKER_FLAGS_INIT
  "${STATIC_FLAGS} -Wl,--subsystem,console -Wl,--major-subsystem-version,5 -Wl,--minor-subsystem-version,1")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
