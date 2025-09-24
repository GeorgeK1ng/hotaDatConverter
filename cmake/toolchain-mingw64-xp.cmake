# MinGW-w64 64-bit toolchain targeting Windows XP x64 (NT 5.2), static link
set(CMAKE_SYSTEM_NAME Windows)

# Compilers from MSYS2 MinGW64
set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)

set(CMAKE_GENERATOR "Ninja" CACHE STRING "")

# XP x64 uses NT 5.2 (Server 2003 kernel). Use 0x0502 and subsystem 5.02.
set(XP_DEFS "-DWINVER=0x0502 -D_WIN32_WINNT=0x0502")
set(STATIC_FLAGS "-static -static-libgcc -static-libstdc++")

set(CMAKE_C_FLAGS_INIT   "${XP_DEFS} -Os")
set(CMAKE_CXX_FLAGS_INIT "${XP_DEFS} -Os")

set(CMAKE_EXE_LINKER_FLAGS_INIT
  "${STATIC_FLAGS} -Wl,--subsystem,console,5.02")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
