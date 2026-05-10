cmake_minimum_required(VERSION 3.16)
project(nativeview C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Generate version header from versions.json before any modules are built
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateVersion.cmake)

# Linux: expose POSIX prototypes (clock_gettime, etc.) on glibc 2.34+.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_compile_definitions(_GNU_SOURCE)
endif()

# =============================================================================
# Shared library option (declared early so subdirectories inherit PIC flag)
# =============================================================================

option(NV_BUILD_SHARED "Build nativeview as a shared library (.dll/.so/.dylib)" OFF)

# Static archives linked into a shared lib must be position-independent on ELF
# platforms.  Set this before any add_subdirectory() calls so every module
# picks it up.
if(NV_BUILD_SHARED AND UNIX AND NOT APPLE)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# =============================================================================
# Build Type Configuration
# =============================================================================

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Debug/Release flags (GCC/Clang; MSVC uses its own defaults)
if(NOT MSVC)
  set(CMAKE_C_FLAGS_DEBUG "-g -DNV_DEBUG -O0 -Wall -Wextra")
  set(CMAKE_C_FLAGS_RELEASE "-Oz -DNDEBUG -ffunction-sections -fdata-sections")
else()
  set(CMAKE_C_FLAGS_DEBUG "/Zi /Od /DNV_DEBUG")
  set(CMAKE_C_FLAGS_RELEASE "/O2 /DNDEBUG")
endif()

# Platform-specific linker flags for Release
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  if(APPLE)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,-dead_strip")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,-dead_strip")
  elseif(UNIX AND NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,--gc-sections")
  endif()
endif()

# =============================================================================
# Module Build
#
# Each module is an independent CMake project under modules/.
# Dependency order (DAG, no cycles):
#
#   nv-core → nv-ipc → nv-ops → nv-runtime → nv-platform-{mac,win,linux}
#                    └→ nv-ui → nv-designer
#   nv-sdk (meta: nativeview + nv-ui)
#
# NV_FULL_LINK_TARGET: the target tests should link against to get the full
# stack including the platform backend. Passed into each module as a cache var.
# =============================================================================

# Optional UI stack (nv-ui, nv-designer, nv-sdk).
# Some embedders only need the core nativeview windowing/runtime and do not
# ship a Vue bundle. Allow them to disable the UI modules to avoid a hard Vue
# dependency during configuration.
option(NV_BUILD_UI "Build nv-ui and dependent modules (requires Vue bundle)" ON)

# Determine platform module name before subdirectories are processed.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NvDetectWindowsPeToolchain.cmake)
if(APPLE)
  set(NV_PLATFORM_MODULE nv-platform-mac)
elseif(NV_WINDOWS_PE_TOOLCHAIN)
  set(NV_PLATFORM_MODULE nv-platform-win)
elseif(UNIX AND NOT APPLE)
  set(NV_PLATFORM_MODULE nv-platform-linux)
endif()

# Tests that need the full stack link against nativeview (defined after platform)
# We use a deferred approach: tests are added after the platform module is built.
# NV_FULL_STACK_TESTS collects test definitions; they are registered at the end.
set(NV_FULL_STACK_TESTS "" CACHE INTERNAL "Tests needing full platform link")

add_subdirectory(modules/nv-core)
add_subdirectory(modules/nv-http)
add_subdirectory(modules/nv-ipc)
add_subdirectory(modules/nv-ops)
add_subdirectory(modules/nv-runtime)
# When building nativeView standalone, set WebView2 fetch root (elkotobi sets it when embedding)
if(NV_WINDOWS_PE_TOOLCHAIN AND NOT DEFINED WEBVIEW2_FETCH_ROOT)
  set(WEBVIEW2_FETCH_ROOT "${CMAKE_BINARY_DIR}" CACHE PATH "Root dir for WebView2 SDK fetch")
endif()
add_subdirectory(modules/${NV_PLATFORM_MODULE})

# =============================================================================
# Convenience alias: "nativeview" target = platform module + full stack
# =============================================================================

add_library(nativeview INTERFACE)
# MinGW GNU ld resolves static .a left-to-right once. CMake still orders
# nv-platform-* before nv-runtime on the link line (dependency graph), so
# nv_window_platform_* stay undefined. Group the in-tree archives so ld
# rescans until satisfied. Keep WebView2 + Win32 libs on nv-platform-*
# (pulled next to that target inside the group; DLL import lib is fine here).
# Same issue on Linux with GNU ld.
if((WIN32 OR (UNIX AND NOT APPLE)) AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(nativeview INTERFACE
    "-Wl,--start-group"
    nv-runtime
    ${NV_PLATFORM_MODULE}
    nv-ops
    nv-ipc
    nv-core
    "-Wl,--end-group"
  )
else()
  target_link_libraries(nativeview INTERFACE ${NV_PLATFORM_MODULE})
endif()

# =============================================================================
# Shared library: nativeview.dll / libnativeview.so / libnativeview.dylib
#
# Build with -DNV_BUILD_SHARED=ON.  Compiles with NV_SHARED_BUILD so NV_API
# exports symbols.  Pascal/C consumers linking the DLL should define NV_SHARED
# when compiling code that includes nativeview headers (imports on Windows).
# Static linking continues to use the `nativeview` INTERFACE target (unchanged).
# =============================================================================

if(NV_BUILD_SHARED)
  add_library(nativeview_shared SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/nv_link_shim.c
  )
  target_compile_definitions(nativeview_shared PRIVATE NV_SHARED_BUILD)
  target_include_directories(nativeview_shared
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
  )
  set_target_properties(nativeview_shared PROPERTIES
    OUTPUT_NAME nativeview
  )

  if(APPLE)
    foreach(_nv_slib nv-core nv-ipc nv-ops nv-runtime ${NV_PLATFORM_MODULE})
      target_link_options(nativeview_shared PRIVATE
        "LINKER:-force_load,$<TARGET_FILE:${_nv_slib}>"
      )
    endforeach()
    target_link_libraries(nativeview_shared PRIVATE ${NV_PLATFORM_MODULE})
  elseif(MSVC)
    foreach(_nv_slib nv-core nv-ipc nv-ops nv-runtime ${NV_PLATFORM_MODULE})
      target_link_libraries(nativeview_shared PRIVATE
        "/WHOLEARCHIVE:$<TARGET_FILE:${_nv_slib}>"
      )
    endforeach()
    if(NV_WINDOWS_PE_TOOLCHAIN)
      target_link_libraries(nativeview_shared PRIVATE
        ${WEBVIEW2_LIBRARY}
        user32
        ole32
        shell32
        uuid
        runtimeobject
        propsys
        shlwapi
        gdiplus
      )
    endif()
  else()
    # MinGW, Linux (GCC/Clang with GNU or LLD)
    target_link_libraries(nativeview_shared PRIVATE
      "-Wl,--whole-archive"
      nv-runtime
      ${NV_PLATFORM_MODULE}
      nv-ops
      nv-ipc
      nv-core
      "-Wl,--no-whole-archive"
    )
    target_link_libraries(nativeview_shared PRIVATE ${NV_PLATFORM_MODULE})
  endif()
endif()

# =============================================================================
# Planned UI modules — added optionally so the build does not break when
# sources are not yet present. Order matters: nv-ui before nv-designer,
# both before nv-sdk.
# =============================================================================

if(NV_BUILD_UI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-ui/CMakeLists.txt")
  add_subdirectory(modules/nv-ui)
endif()
if(NV_BUILD_UI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-designer/CMakeLists.txt")
  add_subdirectory(modules/nv-designer)
endif()
if(NV_BUILD_UI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-sdk/CMakeLists.txt")
  add_subdirectory(modules/nv-sdk)
endif()

# =============================================================================
# Tests
#
# nv-core tests: self-contained, registered by modules/nv-core/CMakeLists.txt
# nv-ipc/nv-ops/nv-runtime tests: need full platform stack, registered here.
# Set NV_BUILD_TESTS=OFF when embedding (e.g. from elkotobi) to avoid building tests.
# =============================================================================

option(NV_BUILD_TESTS "Build nativeView unit tests" ON)

if(NV_BUILD_TESTS)
enable_testing()

set(NV_FULL_STACK_TESTS
  test_ipc
  test_window
  test_window_manager
  test_integration
  test_op_app
  test_op_app_menu
  test_op_clipboard
  test_op_dialog
  test_op_fs
  test_op_fs_watch
  test_op_fs_watch_integration
  test_op_ipc_bus
  test_op_notification
  test_op_screen
  test_op_shell
  test_op_tray
  test_op_window
  test_op_window_menu
  test_op_hotkey
  test_op_windows
)

# Map each test to its module source directory
set(test_ipc_DIR              modules/nv-ipc)
set(test_window_DIR           modules/nv-runtime)
set(test_window_manager_DIR   modules/nv-runtime)
set(test_integration_DIR      modules/nv-runtime)
set(test_op_app_DIR           modules/nv-ops)
set(test_op_app_menu_DIR      modules/nv-ops)
set(test_op_clipboard_DIR     modules/nv-ops)
set(test_op_dialog_DIR        modules/nv-ops)
set(test_op_fs_DIR            modules/nv-ops)
set(test_op_fs_watch_DIR            modules/nv-ops)
set(test_op_fs_watch_integration_DIR modules/nv-ops)
set(test_op_ipc_bus_DIR       modules/nv-ops)
set(test_op_notification_DIR  modules/nv-ops)
set(test_op_screen_DIR        modules/nv-ops)
set(test_op_shell_DIR         modules/nv-ops)
set(test_op_tray_DIR          modules/nv-ops)
set(test_op_window_DIR        modules/nv-ops)
set(test_op_window_menu_DIR   modules/nv-ops)
set(test_op_hotkey_DIR        modules/nv-ops)
set(test_op_windows_DIR       modules/nv-ops)

foreach(t ${NV_FULL_STACK_TESTS})
  set(t_dir ${${t}_DIR})
  add_executable(${t} ${t_dir}/tests/${t}.c ${t_dir}/tests/test_helpers.c)
  target_link_libraries(${t} nativeview)
  target_include_directories(${t} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-ops/src
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-runtime/src
  )
  target_compile_definitions(${t} PRIVATE NV_TEST)
  add_test(NAME ${t} COMMAND ${t})
endforeach()
endif(NV_BUILD_TESTS)

# =============================================================================
# Benchmarks
# =============================================================================

add_executable(bench_ipc benchmarks/bench_ipc.c)
target_link_libraries(bench_ipc nativeview)
target_include_directories(bench_ipc PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(bench_shm benchmarks/bench_shm.c)
target_link_libraries(bench_shm nv-ipc)
target_include_directories(bench_shm PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# =============================================================================
# Examples
# =============================================================================

add_executable(hello examples/hello/main.c)
target_link_libraries(hello nativeview)
target_include_directories(hello PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# counter_ui — nv-ui component model end-to-end example
if(NV_BUILD_UI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-ui/CMakeLists.txt")
  add_executable(counter_ui examples/counter_ui/main.c)
  target_link_libraries(counter_ui PRIVATE nativeview nv-ui)
  target_include_directories(counter_ui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-ui/include
  )
  # vue_slot — NV_COMP_VUE slot with createApp per slot
  add_executable(vue_slot examples/vue_slot/main.c)
  target_link_libraries(vue_slot PRIVATE nativeview nv-ui)
  target_include_directories(vue_slot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-ui/include
  )
  # todo_app — dynamic component list (add/remove items)
  add_executable(todo_app examples/todo_app/main.c)
  target_link_libraries(todo_app PRIVATE nativeview nv-ui)
  target_include_directories(todo_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-ui/include
  )
  # settings_form — all widget types (label, input, checkbox, select, slider, textarea)
  add_executable(settings_form examples/settings_form/main.c)
  target_link_libraries(settings_form PRIVATE nativeview nv-ui)
  target_include_directories(settings_form PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-ui/include
  )
  # live_data — slider + live display (Phase 4 use case; shm path when platform supports)
  add_executable(live_data examples/live_data/main.c)
  target_link_libraries(live_data PRIVATE nativeview nv-ui)
  target_include_directories(live_data PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-ui/include
  )
endif()

# Demo
add_subdirectory(demo)

# =============================================================================
# pkg-config
# =============================================================================

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/nativeview.pc.in
  ${CMAKE_CURRENT_BINARY_DIR}/nativeview.pc
  @ONLY
)

# =============================================================================
# Amalgamation Target
# =============================================================================

if(APPLE)
  set(AMALG_PLATFORM mac)
elseif(WIN32)
  set(AMALG_PLATFORM win)
else()
  set(AMALG_PLATFORM linux)
endif()

add_custom_target(amalgamate
  COMMAND ${CMAKE_COMMAND} -E echo "Running amalgamate script for ${AMALG_PLATFORM}..."
  COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/amalgamate.py --platform ${AMALG_PLATFORM} --output dist
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Generating single-file amalgamation"
)

# =============================================================================
# Installation
# =============================================================================

install(
  TARGETS nativeview
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
)

if(NV_BUILD_SHARED)
  install(
    TARGETS nativeview_shared
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
  )
endif()

install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
  DESTINATION include
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/nativeview.pc
  DESTINATION lib/pkgconfig
)
