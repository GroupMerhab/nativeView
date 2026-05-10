cmake_minimum_required(VERSION 3.16)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Executable
add_executable(nv_demo
  main.c
)

# Link against nativeview (static by default unless NV_SHARED is ON in parent)
target_link_libraries(nv_demo PRIVATE nativeview)

# Include dirs (public headers are propagated by nativeview, but keep local safe)
target_include_directories(nv_demo PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../include
  ${CMAKE_CURRENT_LIST_DIR}/../src
)

# Copy the entire demo folder next to the executable after build
find_program(PYTHON_EXE NAMES python3 python python3.exe python.exe)
if(NOT PYTHON_EXE)
  message(WARNING "Python not found; nv_demo nativeview.js will not be built. Install Python and reconfigure.")
  set(NV_DEMO_PYTHON_CMD "${CMAKE_COMMAND}" -E echo "Skipped: Python not found")
  set(NV_DEMO_JS_ARGS "")
else()
  set(NV_DEMO_PYTHON_CMD "${PYTHON_EXE}" "${CMAKE_CURRENT_LIST_DIR}/../tools/js_build.py"
          --src "${CMAKE_CURRENT_LIST_DIR}/../js/src"
          --out "$<TARGET_FILE_DIR:nv_demo>/dist/nativeview.js")
endif()
add_custom_command(TARGET nv_demo POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_LIST_DIR}"
          "$<TARGET_FILE_DIR:nv_demo>/demo"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:nv_demo>/dist"
  COMMAND ${NV_DEMO_PYTHON_CMD} ${NV_DEMO_JS_ARGS}
  COMMENT "Copying demo/ and building nativeview.js next to nv_demo"
)
