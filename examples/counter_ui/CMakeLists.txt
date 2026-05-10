cmake_minimum_required(VERSION 3.16)
project(counter_ui C)
set(CMAKE_C_STANDARD 11)

add_executable(counter_ui main.c)
target_link_libraries(counter_ui PRIVATE nv-sdk)
target_include_directories(counter_ui PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/nv-ui/include
)
