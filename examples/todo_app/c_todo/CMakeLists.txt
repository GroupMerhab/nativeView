# examples/todo_app/c_todo — Vue UI embedded from ../ui (expects nativeView superbuild as subdirectory).
cmake_minimum_required(VERSION 3.16)

option(NV_TODO_SKIP_UI_BUILD "Skip npm/Vite; embed ui/fallback_index.html only" OFF)

if(NOT TARGET SQLite::SQLite3)
  add_subdirectory("${CMAKE_SOURCE_DIR}/sqlite" "${CMAKE_BINARY_DIR}/sqlite")
endif()

find_program(TODO_PYTHON NAMES python3 python REQUIRED)
find_program(TODO_NPM NAMES npm DOC "npm for Vue UI build")

set(TODO_HTML_GEN "${CMAKE_CURRENT_BINARY_DIR}/todo_html.c")
set(TODO_EMBED_PY "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_html.py")

if(NV_TODO_SKIP_UI_BUILD)
  add_custom_command(
    OUTPUT "${TODO_HTML_GEN}"
    COMMAND "${TODO_PYTHON}" "${TODO_EMBED_PY}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../ui/fallback_index.html"
            "${TODO_HTML_GEN}" nv_todo_html
    DEPENDS "${TODO_EMBED_PY}" "${CMAKE_CURRENT_SOURCE_DIR}/../ui/fallback_index.html"
    COMMENT "Embedding fallback HTML for todo_app"
    VERBATIM
  )
else()
  set(TODO_UI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../ui")
  add_custom_command(
    OUTPUT "${TODO_HTML_GEN}"
    COMMAND "${TODO_NPM}" install --prefix "${TODO_UI_ROOT}"
    COMMAND "${TODO_NPM}" run build --prefix "${TODO_UI_ROOT}"
    COMMAND "${TODO_PYTHON}" "${TODO_EMBED_PY}"
            "${TODO_UI_ROOT}/dist/index.html"
            "${TODO_HTML_GEN}" nv_todo_html
    DEPENDS
      "${TODO_EMBED_PY}"
      "${TODO_UI_ROOT}/package.json"
      "${TODO_UI_ROOT}/vite.config.js"
      "${TODO_UI_ROOT}/index.html"
      "${TODO_UI_ROOT}/src/main.js"
      "${TODO_UI_ROOT}/src/App.vue"
      "${TODO_UI_ROOT}/src/theme.css"
      "${TODO_UI_ROOT}/src/bridge.js"
      "${TODO_UI_ROOT}/src/store/todos.js"
      "${TODO_UI_ROOT}/src/store/categories.js"
      "${TODO_UI_ROOT}/src/components/CategoryBadge.vue"
      "${TODO_UI_ROOT}/src/components/TodoItem.vue"
      "${TODO_UI_ROOT}/src/components/TodoForm.vue"
      "${TODO_UI_ROOT}/src/components/TodoFilters.vue"
      "${TODO_UI_ROOT}/src/components/TodoList.vue"
      "${TODO_UI_ROOT}/src/components/SideToolbar.vue"
      "${TODO_UI_ROOT}/src/views/ArchiveView.vue"
      "${TODO_UI_ROOT}/src/views/DoneView.vue"
      "${TODO_UI_ROOT}/src/bridge.test.js"
      "${TODO_UI_ROOT}/src/CategoryBadge.test.js"
    COMMENT "Building Vue UI + embedding index.html for todo_app"
    VERBATIM
  )
endif()

add_library(todo_store STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/todo_store.c"
)
target_include_directories(todo_store
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(todo_store PUBLIC SQLite::SQLite3)

add_library(todo_bridge STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/todo_bridge.c"
)
target_include_directories(todo_bridge
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/include"
)
target_link_libraries(todo_bridge PUBLIC todo_store nv-core)

add_library(todo_app_impl OBJECT
  "${CMAKE_CURRENT_SOURCE_DIR}/src/todo_app.c"
  "${TODO_HTML_GEN}"
)
target_include_directories(todo_app_impl PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/include"
)

add_executable(todo_app
  "${CMAKE_CURRENT_SOURCE_DIR}/main.c"
  $<TARGET_OBJECTS:todo_app_impl>
)
# $<LINK_GROUP:RESCAN,...> is not supported for plain C targets on some toolchains
# (e.g. AppleClang) and does not apply across INTERFACE targets like `nativeview`.
# Link order: full stack first, then app-side static libs (mirrors examples/nim).
target_link_libraries(todo_app PRIVATE nativeview todo_bridge todo_store)

add_library(todo_app_core STATIC $<TARGET_OBJECTS:todo_app_impl>)
target_include_directories(todo_app_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/include"
)
target_link_libraries(todo_app_core PUBLIC todo_bridge todo_store)

if(NV_BUILD_TESTS)
  add_executable(test_todo_store
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_todo_store.c"
  )
  target_link_libraries(test_todo_store PRIVATE todo_store)
  add_test(NAME test_todo_store COMMAND test_todo_store)

  add_executable(test_todo_bridge
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_todo_bridge.c"
  )
  target_link_libraries(test_todo_bridge PRIVATE todo_bridge)
  add_test(NAME test_todo_bridge COMMAND test_todo_bridge)
endif()
