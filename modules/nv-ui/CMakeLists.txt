# =============================================================================
# nv-ui — Reactive component model (composite)
#
# Single nv_component_t node type with type enum; paint/destroy/resize
# switch on type and walk the tree. No vtables.
#
# Dependencies: nv-ipc, nv-runtime
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(nv-ui C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(NV_ROOT_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")

if(NOT TARGET nv-ipc)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../nv-ipc
                   ${CMAKE_CURRENT_BINARY_DIR}/nv-ipc)
endif()
if(NOT TARGET nv-runtime)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../nv-runtime
                   ${CMAKE_CURRENT_BINARY_DIR}/nv-runtime)
endif()

add_library(nv-ui STATIC
  src/nv_component.c
  src/nv_widget.c
  src/nv_widget_flush.c
  src/nv_widget_paint.c
  src/nv_vue_slot.c
  # nv_vue_bundle.c added below (generated)
)

target_link_libraries(nv-ui PUBLIC nv-ipc nv-runtime)
target_compile_definitions(nv-ui PRIVATE NV_TEST)  # nv_canvas_create_for_test for tests

# Embed Vue locally (no CDN) — looks for js/vue.js or assets/vue.global.prod.js
set(NV_VUE_JS "")
if(EXISTS "${CMAKE_SOURCE_DIR}/js/vue.js")
  set(NV_VUE_JS "${CMAKE_SOURCE_DIR}/js/vue.js")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/vue.global.prod.js")
  set(NV_VUE_JS "${CMAKE_CURRENT_SOURCE_DIR}/assets/vue.global.prod.js")
endif()
set(NV_VUE_BUNDLE_C "${CMAKE_CURRENT_BINARY_DIR}/nv_vue_bundle.c")
if(NV_VUE_JS)
  find_package(Python3 QUIET COMPONENTS Interpreter)
  if(NOT Python3_EXECUTABLE)
    set(Python3_EXECUTABLE python3)
  endif()
  add_custom_command(OUTPUT "${NV_VUE_BUNDLE_C}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/embed_vue.py"
      "${NV_VUE_JS}" "${NV_VUE_BUNDLE_C}"
    DEPENDS "${NV_VUE_JS}" "${CMAKE_SOURCE_DIR}/tools/embed_vue.py"
    COMMENT "Embedding Vue into nv_vue_bundle.c (no CDN)")
else()
  message(FATAL_ERROR "Vue bundle required. Put vue.global.prod.js in js/vue.js or modules/nv-ui/assets/")
endif()
target_sources(nv-ui PRIVATE "${NV_VUE_BUNDLE_C}")

target_include_directories(nv-ui
  PUBLIC
    ${NV_ROOT_INCLUDE}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/nv-runtime/src
)

enable_testing()

set(NV_UI_TESTS test_object test_component)
if(TARGET nativeview)
  list(APPEND NV_UI_TESTS test_widget)  # needs platform for nv_ui_flush
endif()
foreach(t ${NV_UI_TESTS})
  add_executable(${t} tests/${t}.c)
  target_link_libraries(${t} nv-ui)
  if(t STREQUAL test_widget AND TARGET nativeview)
    target_link_libraries(${t} nativeview)
  endif()
  target_include_directories(${t} PRIVATE ${NV_ROOT_INCLUDE} ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_definitions(${t} PRIVATE NV_TEST)
  add_test(NAME ${t} COMMAND ${t})
endforeach()
