# MinGW-w64 32-bit toolchain targeting Windows XP (NT 5.1), static link
set(CMAKE_SYSTEM_NAME Windows)

# Compilers from MSYS2 MinGW32
set(CMAKE_C_COMPILER   i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)

# Force MinGW generator (Ninja is fine too)
set(CMAKE_GENERATOR "Ninja" CACHE STRING "")

# Target XP 32-bit: WINVER/NT, subsystem 5.01
set(XP_DEFS "-DWINVER=0x0501 -D_WIN32_WINNT=0x0501")
set(STATIC_FLAGS "-static -static-libgcc -static-libstdc++")

# Optimize for size; avoid new APIs
set(CMAKE_C_FLAGS_INIT   "${XP_DEFS} -Os")
set(CMAKE_CXX_FLAGS_INIT "${XP_DEFS} -Os")

# Linker: set subsystem version to 5.01 and link statically
set(CMAKE_EXE_LINKER_FLAGS_INIT
  "${STATIC_FLAGS} -Wl,--subsystem,console,5.01")

# Make Release the default
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
