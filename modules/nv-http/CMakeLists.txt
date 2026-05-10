# =============================================================================
# nv-http — HTTP server module (socket layer, protocol, routing)
#
# Provides: Platform-neutral TCP sockets, HTTP parsing, routing, file serving.
#
# Dependencies: nv-core (arena, util, log)
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(nv-http C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(NV_ROOT_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
set(NV_ROOT_SRC     "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

if(NOT TARGET nv-core)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../nv-core
                   ${CMAKE_CURRENT_BINARY_DIR}/nv-core)
endif()

# Platform-specific socket implementation
if(WIN32)
  set(SOCKET_PLATFORM_SOURCES src/platform/socket_layer_windows.c)
  set(SOCKET_PLATFORM_LIBS ws2_32)
else()
  set(SOCKET_PLATFORM_SOURCES src/platform/socket_layer_posix.c)
  set(SOCKET_PLATFORM_LIBS "")
endif()

add_library(nv-http STATIC
  src/socket_layer_common.c
  src/http_protocol.c
  src/http_core.c
  src/mime_types.c
  src/file_server.c
  src/url_parser.c
  ${SOCKET_PLATFORM_SOURCES}
)

target_include_directories(nv-http
  PUBLIC
    ${NV_ROOT_INCLUDE}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${NV_ROOT_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(nv-http
  PUBLIC
    nv-core
  PRIVATE
    ${SOCKET_PLATFORM_LIBS}
)

# =============================================================================
# Tests
# =============================================================================

enable_testing()

add_executable(test_socket_layer tests/test_socket_layer.c)
target_link_libraries(test_socket_layer nv-http)
target_include_directories(test_socket_layer PRIVATE
  ${NV_ROOT_INCLUDE}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_definitions(test_socket_layer PRIVATE NV_TEST)
add_test(NAME test_socket_layer COMMAND test_socket_layer)

add_executable(test_http_protocol tests/test_http_protocol.c)
target_link_libraries(test_http_protocol nv-http)
target_include_directories(test_http_protocol PRIVATE
  ${NV_ROOT_INCLUDE}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_definitions(test_http_protocol PRIVATE NV_TEST)
add_test(NAME test_http_protocol COMMAND test_http_protocol)

add_executable(test_http_core tests/test_http_core.c)
target_link_libraries(test_http_core nv-http)
target_include_directories(test_http_core PRIVATE
  ${NV_ROOT_INCLUDE}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_definitions(test_http_core PRIVATE NV_TEST)
add_test(NAME test_http_core COMMAND test_http_core)

add_executable(test_mime_types tests/test_mime_types.c)
target_link_libraries(test_mime_types nv-http)
target_include_directories(test_mime_types PRIVATE
  ${NV_ROOT_INCLUDE}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_definitions(test_mime_types PRIVATE NV_TEST)
add_test(NAME test_mime_types COMMAND test_mime_types)

add_executable(test_file_server tests/test_file_server.c)
target_link_libraries(test_file_server nv-http)
target_include_directories(test_file_server PRIVATE
  ${NV_ROOT_INCLUDE}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_definitions(test_file_server PRIVATE
  NV_TEST
  "NVHTTP_FIXTURES_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures\"")
add_test(NAME test_file_server COMMAND test_file_server)

add_executable(test_url_parser tests/test_url_parser.c)
target_link_libraries(test_url_parser nv-http)
target_include_directories(test_url_parser PRIVATE
  ${NV_ROOT_INCLUDE}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_definitions(test_url_parser PRIVATE NV_TEST)
add_test(NAME test_url_parser COMMAND test_url_parser)

# =============================================================================
# CLI: static file server on directory + port
# =============================================================================

add_executable(http_server_cli examples/http_server_cli.c)
target_link_libraries(http_server_cli nv-http)
target_include_directories(http_server_cli PRIVATE
  ${NV_ROOT_INCLUDE}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
