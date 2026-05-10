# NvDetectWindowsPeToolchain.cmake
#
# Sets NV_WINDOWS_PE_TOOLCHAIN for nativeView and embedders (e.g. elkotobi).
# When TRUE, the build targets Windows PE (MSVC, MinGW-w64, Clang with a
# Windows triple, etc.). CMake's WIN32 is sufficient for that distinction.
#
# MSYS2 MinGW sets both WIN32 and UNIX; platform selection must check WIN32
# (via this flag) before the generic UNIX branch in nativeView/CMakeLists.txt.

include_guard(GLOBAL)

if(WIN32)
  set(NV_WINDOWS_PE_TOOLCHAIN ON CACHE INTERNAL "Target is Windows PE (MSVC/MinGW/...)")
else()
  set(NV_WINDOWS_PE_TOOLCHAIN OFF CACHE INTERNAL "Target is Windows PE (MSVC/MinGW/...)")
endif()
