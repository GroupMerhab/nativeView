# =============================================================================
# nv-designer — Visual form designer tool
#
# nv-designer is a standalone executable (not a library) that reads a form
# layout and emits a pair of C files:
#
#   myform_ui.c   — designer-owned: NV_DESIGNER_BEGIN/END block
#   myform.c      — user-owned: includes myform_ui.c, holds event handlers
#
# The generated myform_ui.c calls nv-ui public API (nv_button_new, etc.).
# nv-designer itself depends on nv-ui only for header/type information;
# it does NOT link the full platform stack at design time.
#
# Status: PLANNED — this is a stub. Uncomment the add_executable line when
#         src/main.c is implemented.
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(nv-designer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Pull in nv-ui when built standalone
if(NOT TARGET nv-ui)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../nv-ui
                   ${CMAKE_CURRENT_BINARY_DIR}/nv-ui)
endif()

# =============================================================================
# Tool executable — uncomment when src/main.c is implemented
# =============================================================================

# add_executable(nv-designer src/main.c)
# target_link_libraries(nv-designer PRIVATE nv-ui)
# target_include_directories(nv-designer PRIVATE
#   ${CMAKE_CURRENT_SOURCE_DIR}/../../include
#   ${CMAKE_CURRENT_SOURCE_DIR}/../nv-ui/include
# )
# install(TARGETS nv-designer RUNTIME DESTINATION bin)
