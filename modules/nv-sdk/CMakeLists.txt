# =============================================================================
# nv-sdk — Meta-package for application developers
#
# nv-sdk is a pure INTERFACE library with no source files of its own.
# It links the full platform stack (nativeview) together with the component
# model (nv-ui) so that application code only needs to link one target:
#
#   target_link_libraries(my_app PRIVATE nv-sdk)
#
# Dependency graph (nv-sdk is the leaf):
#
#   nv-core → nv-ipc → nv-ops → nv-runtime → nv-platform-{mac,win,linux}
#                    └→ nv-ui → nv-designer
#   nv-sdk (meta: nativeview + nv-ui)
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(nv-sdk C)

# nv-sdk has no C standard requirement of its own — it is purely INTERFACE.

# =============================================================================
# Meta-package — links nativeview (full platform stack) + nv-ui
#
# Both targets must already exist (added by the root CMakeLists.txt before
# this subdirectory is processed). The guard prevents a confusing CMake error
# when nv-sdk is accidentally added before its dependencies.
# =============================================================================

add_library(nv-sdk INTERFACE)

if(TARGET nativeview AND TARGET nv-ui)
  target_link_libraries(nv-sdk INTERFACE nativeview nv-ui)
else()
  message(WARNING
    "nv-sdk: 'nativeview' and/or 'nv-ui' targets not found. "
    "Make sure nv-sdk is added AFTER nativeview and nv-ui in the root "
    "CMakeLists.txt. nv-sdk will be an empty INTERFACE library.")
endif()

# Expose the public headers of all constituent modules to consumers
target_include_directories(nv-sdk
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../nv-ui/include
)
