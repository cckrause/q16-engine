# CMake toolchain file for cross-compiling to Win95/98 (i686 PE32)
# Usage: cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw32.cmake

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)

# MinGW cross-compiler (install via: brew install mingw-w64)
set(CMAKE_C_COMPILER   i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
set(CMAKE_RC_COMPILER  i686-w64-mingw32-windres)

# Use old msvcrt.dll instead of UCRT (UCRT doesn't exist on Win95/98)
set(CMAKE_C_FLAGS_INIT   "-mcrtdll=msvcrt-os -Wall -Wextra -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS_INIT "-mcrtdll=msvcrt-os -Wall -Wextra -Wno-unused-parameter")

# Search paths
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
