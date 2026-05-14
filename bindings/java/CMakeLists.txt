# Desktop JNI glue for bindings/java (built only when NV_BUILD_JAVA_JNI=ON).
#
# Requires NV_BUILD_SHARED=ON and a JDK (CMake FindJNI).

find_package(JNI REQUIRED)

add_library(nativeview_jni SHARED
  src/main/c/nativeview_jni_api.c
  src/main/c/nativeview_jni_io.c
  src/main/c/nativeview_jni_hooks.c
)

target_include_directories(nativeview_jni PRIVATE
  ${JNI_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(nativeview_jni PRIVATE nativeview_shared)

if(MSVC)
  target_link_libraries(nativeview_jni PRIVATE ${JNI_LIBRARIES})
endif()

set_target_properties(nativeview_jni PROPERTIES
  OUTPUT_NAME nativeview_jni
)

if(APPLE)
  set_target_properties(nativeview_jni PROPERTIES
    MACOSX_RPATH ON
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
  )
elseif(UNIX)
  set_target_properties(nativeview_jni PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
  )
endif()
