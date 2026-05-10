# =============================================================================
# FetchWebView2.cmake — Download and configure WebView2 SDK when missing
#
# Finds WebView2 in standard locations (VS, NuGet packages). If not found,
# downloads Microsoft.Web.WebView2 NuGet package and configures include/lib.
#
# Output:
#   WEBVIEW2_INCLUDE_DIR — path to WebView2.h
#   WEBVIEW2_LIBRARY     — WebView2LoaderStatic.lib (static loader)
#   WEBVIEW2_FOUND       — TRUE when SDK is available
# =============================================================================

set(WEBVIEW2_VERSION "1.0.2592.51")
set(WEBVIEW2_NUGET_URL "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VERSION}")

# When CMake's built-in HTTPS client fails (common on Windows: "SSL connect error"),
# fallbacks try curl.exe / PowerShell (Schannel). Last resort: skip TLS verify (INSECURE).
option(WEBVIEW2_DOWNLOAD_SKIP_TLS_VERIFY
  "If all HTTPS download methods fail, retry without TLS certificate verification (INSECURE; use only on broken SSL/proxy setups)"
  OFF)

# Target ABI for WebView2LoaderStatic.lib (must match link /machine, not the host).
# Nested `project()` under nativeView can reset CMAKE_SYSTEM_PROCESSOR to the host
# (e.g. AMD64) during MSVC ARM64 cross-builds; elkotobi sets ELKOTOBI_MSVC_TARGET_ARCH.
set(_nv_wv2_loader_arch "x64")
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(_nv_wv2_loader_arch "x86")
elseif(ELKOTOBI_MSVC_TARGET_ARCH STREQUAL "arm64")
  set(_nv_wv2_loader_arch "arm64")
elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64" OR CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64")
  set(_nv_wv2_loader_arch "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64|AARCH64)$")
  set(_nv_wv2_loader_arch "arm64")
endif()

# -----------------------------------------------------------------------------
# 1. Try to find WebView2 in standard locations
# -----------------------------------------------------------------------------
set(_WEBVIEW2_FOUND FALSE)

# Check explicit path (e.g. -DWEBVIEW2_ROOT=/path)
if(DEFINED WEBVIEW2_ROOT)
  set(_inc "${WEBVIEW2_ROOT}/build/native/include")
  set(_lib_x64 "${WEBVIEW2_ROOT}/build/native/x64/WebView2LoaderStatic.lib")
  set(_lib_x86 "${WEBVIEW2_ROOT}/build/native/x86/WebView2LoaderStatic.lib")
  set(_lib_arm64 "${WEBVIEW2_ROOT}/build/native/arm64/WebView2LoaderStatic.lib")
  if(EXISTS "${_inc}/WebView2.h")
    set(WEBVIEW2_INCLUDE_DIR "${_inc}" CACHE PATH "WebView2 include directory")
    if(_nv_wv2_loader_arch STREQUAL "arm64")
      set(WEBVIEW2_LIBRARY "${_lib_arm64}" CACHE FILEPATH "WebView2 library")
    elseif(_nv_wv2_loader_arch STREQUAL "x86")
      set(WEBVIEW2_LIBRARY "${_lib_x86}" CACHE FILEPATH "WebView2 library")
    else()
      set(WEBVIEW2_LIBRARY "${_lib_x64}" CACHE FILEPATH "WebView2 library")
    endif()
    set(_WEBVIEW2_FOUND TRUE)
  endif()
endif()

# Check common NuGet packages directory (solution/packages folder)
if(NOT _WEBVIEW2_FOUND)
  set(_nuget_paths
    "${CMAKE_BINARY_DIR}/packages/Microsoft.Web.WebView2"
    "${CMAKE_SOURCE_DIR}/packages/Microsoft.Web.WebView2"
    "$ENV{USERPROFILE}/.nuget/packages/microsoft.web.webview2/${WEBVIEW2_VERSION}"
    "$ENV{NUGET_PACKAGES}/microsoft.web.webview2/${WEBVIEW2_VERSION}"
  )
  foreach(_p IN LISTS _nuget_paths)
    set(_inc "${_p}/build/native/include")
    if(EXISTS "${_inc}/WebView2.h")
      set(WEBVIEW2_INCLUDE_DIR "${_inc}" CACHE PATH "WebView2 include directory")
      if(_nv_wv2_loader_arch STREQUAL "arm64")
        set(WEBVIEW2_LIBRARY "${_p}/build/native/arm64/WebView2LoaderStatic.lib" CACHE FILEPATH "WebView2 library")
      elseif(_nv_wv2_loader_arch STREQUAL "x86")
        set(WEBVIEW2_LIBRARY "${_p}/build/native/x86/WebView2LoaderStatic.lib" CACHE FILEPATH "WebView2 library")
      else()
        set(WEBVIEW2_LIBRARY "${_p}/build/native/x64/WebView2LoaderStatic.lib" CACHE FILEPATH "WebView2 library")
      endif()
      set(_WEBVIEW2_FOUND TRUE)
      break()
    endif()
  endforeach()
endif()

# Check if we already have WebView2 via system/VS (find_path for WebView2.h)
if(NOT _WEBVIEW2_FOUND)
  find_path(WEBVIEW2_INCLUDE_DIR NAMES WebView2.h
    PATHS
      "$ENV{ProgramFiles}/Microsoft/WebView2/Include"
      "$ENV{ProgramFiles\(x86\)}/Microsoft/WebView2/Include"
      "C:/Program Files (x86)/Microsoft/WebView2/Include"
      "C:/Program Files/Microsoft/WebView2/Include"
    DOC "WebView2 include directory"
  )
  if(WEBVIEW2_INCLUDE_DIR)
    # Prefer static loader when both exist (MSVC); MinGW override may switch to .dll.lib below.
    # Search target arch first — Lib/x64 often appears before Lib/arm64 on disk.
    set(_wv2_sys_suffixes x64)
    if(_nv_wv2_loader_arch STREQUAL "arm64")
      set(_wv2_sys_suffixes arm64)
    elseif(_nv_wv2_loader_arch STREQUAL "x86")
      set(_wv2_sys_suffixes x86)
    endif()
    find_library(WEBVIEW2_LIBRARY NAMES WebView2LoaderStatic WebView2
      PATHS
        "$ENV{ProgramFiles}/Microsoft/WebView2/Lib"
        "$ENV{ProgramFiles\(x86\)}/Microsoft/WebView2/Lib"
        "C:/Program Files (x86)/Microsoft/WebView2/Lib"
        "C:/Program Files/Microsoft/WebView2/Lib"
      PATH_SUFFIXES ${_wv2_sys_suffixes}
      DOC "WebView2 library"
    )
    if(WEBVIEW2_LIBRARY)
      set(_WEBVIEW2_FOUND TRUE)
    endif()
  endif()
endif()

# -----------------------------------------------------------------------------
# 2. If still not found, download NuGet package (use shared fetch root when set)
# -----------------------------------------------------------------------------
if(NOT _WEBVIEW2_FOUND)
  if(DEFINED WEBVIEW2_FETCH_ROOT)
    # Use fetch root as-is so one download is shared (elkotobi sets this to its build dir)
    set(WEBVIEW2_FETCH_DIR "${WEBVIEW2_FETCH_ROOT}/_deps/webview2-sdk")
  else()
    set(WEBVIEW2_FETCH_DIR "${CMAKE_BINARY_DIR}/_deps/webview2-sdk")
    get_filename_component(WEBVIEW2_FETCH_DIR "${WEBVIEW2_FETCH_DIR}" ABSOLUTE)
  endif()
  set(_deps_dir "${WEBVIEW2_FETCH_DIR}/..")
  set(_nupkg "${_deps_dir}/webview2.nupkg")
  set(_extract_staging "${_deps_dir}/webview2-extract")
  set(_header_check "${WEBVIEW2_FETCH_DIR}/build/native/include/WebView2.h")

  if(NOT EXISTS "${_header_check}")
    message(STATUS "WebView2 SDK not found. Downloading from NuGet...")
    file(MAKE_DIRECTORY "${_deps_dir}")
    file(MAKE_DIRECTORY "${_extract_staging}")

    # Download .nupkg (NuGet package is a zip archive)
    set(_wv2_dl_ok FALSE)
    if(EXISTS "${_nupkg}")
      file(REMOVE "${_nupkg}")
    endif()

    file(DOWNLOAD
      "${WEBVIEW2_NUGET_URL}"
      "${_nupkg}"
      TLS_VERIFY ON
      STATUS _dl_status
    )
    list(GET _dl_status 0 _dl_code)
    if(_dl_code EQUAL 0 AND EXISTS "${_nupkg}")
      set(_wv2_dl_ok TRUE)
    else()
      list(GET _dl_status 1 _dl_err)
      message(STATUS "WebView2: CMake HTTPS download failed (code ${_dl_code}: ${_dl_err}); trying curl/PowerShell...")
    endif()

    # curl.exe on Windows typically uses Schannel and succeeds when CMake's TLS stack does not.
    if(NOT _wv2_dl_ok AND WIN32)
      if(EXISTS "${_nupkg}")
        file(REMOVE "${_nupkg}")
      endif()
      find_program(_wv2_curl NAMES curl curl.exe)
      if(_wv2_curl)
        execute_process(
          COMMAND "${_wv2_curl}" -fsSL --retry 3 -o "${_nupkg}" "${WEBVIEW2_NUGET_URL}"
          RESULT_VARIABLE _wv2_curl_rc
        )
        if(_wv2_curl_rc EQUAL 0 AND EXISTS "${_nupkg}")
          set(_wv2_dl_ok TRUE)
          message(STATUS "WebView2: downloaded via curl (${_wv2_curl})")
        endif()
      endif()
    endif()

    if(NOT _wv2_dl_ok AND WIN32)
      if(EXISTS "${_nupkg}")
        file(REMOVE "${_nupkg}")
      endif()
      # Forward slashes keep PowerShell -Command parsing predictable on Windows paths.
      file(TO_CMAKE_PATH "${_nupkg}" _wv2_nupkg_out)
      string(REPLACE "\\" "/" _wv2_nupkg_out "${_wv2_nupkg_out}")
      execute_process(
        COMMAND powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass
          -Command
            "$ProgressPreference='SilentlyContinue'; [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; try { Invoke-WebRequest -Uri '${WEBVIEW2_NUGET_URL}' -OutFile '${_wv2_nupkg_out}' -UseBasicParsing; exit 0 } catch { exit 1 }"
        RESULT_VARIABLE _wv2_ps_rc
      )
      if(_wv2_ps_rc EQUAL 0 AND EXISTS "${_nupkg}")
        set(_wv2_dl_ok TRUE)
        message(STATUS "WebView2: downloaded via PowerShell (Invoke-WebRequest)")
      endif()
    endif()

    if(NOT _wv2_dl_ok AND WEBVIEW2_DOWNLOAD_SKIP_TLS_VERIFY)
      message(WARNING "WebView2: WEBVIEW2_DOWNLOAD_SKIP_TLS_VERIFY=ON — downloading without TLS certificate verification.")
      if(EXISTS "${_nupkg}")
        file(REMOVE "${_nupkg}")
      endif()
      file(DOWNLOAD
        "${WEBVIEW2_NUGET_URL}"
        "${_nupkg}"
        TLS_VERIFY OFF
        STATUS _dl_status2
      )
      list(GET _dl_status2 0 _dl_code2)
      if(_dl_code2 EQUAL 0 AND EXISTS "${_nupkg}")
        set(_wv2_dl_ok TRUE)
      endif()
    endif()

    if(NOT _wv2_dl_ok)
      message(FATAL_ERROR
        "Failed to download WebView2 SDK from NuGet after CMake, curl, and PowerShell attempts.\n"
        "Fix HTTPS/TLS (proxy, firewall, corporate CA), install the WebView2 SDK so headers/libs are found, "
        "or pass -DWEBVIEW2_ROOT=path/to/extracted/Microsoft.Web.WebView2.${WEBVIEW2_VERSION} "
        "(folder containing build/native/include/WebView2.h).\n"
        "Last resort only: -DWEBVIEW2_DOWNLOAD_SKIP_TLS_VERIFY=ON")
    endif()

    # Extract .nupkg (it is a zip file)
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E tar xf "${_nupkg}"
      WORKING_DIRECTORY "${_extract_staging}"
      RESULT_VARIABLE _extract_result
    )
    if(NOT _extract_result EQUAL 0)
      message(FATAL_ERROR "Failed to extract WebView2 NuGet package")
    endif()

    # NuGet extracts to current dir; move build/ to our fetch dir
    if(EXISTS "${_extract_staging}/build")
      file(MAKE_DIRECTORY "${WEBVIEW2_FETCH_DIR}")
      file(RENAME "${_extract_staging}/build" "${WEBVIEW2_FETCH_DIR}/build")
      set(_webview2_just_downloaded TRUE)
    else()
      message(FATAL_ERROR "WebView2 NuGet package did not contain build/ (extract dir: ${_extract_staging})")
    endif()
    file(REMOVE_RECURSE "${_extract_staging}")
    file(REMOVE "${_nupkg}")
    message(STATUS "WebView2 SDK downloaded to ${WEBVIEW2_FETCH_DIR}")
  endif()

  set(_inc "${WEBVIEW2_FETCH_DIR}/build/native/include")
  # Prefer GLOB to avoid path/EXISTS quirks; fallback to EXISTS
  file(GLOB _wv2_header "${WEBVIEW2_FETCH_DIR}/build/native/include/WebView2.h")
  if(_webview2_just_downloaded OR _wv2_header OR EXISTS "${_inc}/WebView2.h")
    set(_use_fetched TRUE)
  endif()
  if(_use_fetched)
    set(WEBVIEW2_INCLUDE_DIR "${_inc}" CACHE PATH "WebView2 include directory" FORCE)
    set(_lib "${WEBVIEW2_FETCH_DIR}/build/native/${_nv_wv2_loader_arch}/WebView2LoaderStatic.lib")
    if(NOT EXISTS "${_lib}")
      if(_nv_wv2_loader_arch STREQUAL "arm64")
        message(FATAL_ERROR
          "WebView2 SDK at ${WEBVIEW2_FETCH_DIR} has no arm64/WebView2LoaderStatic.lib. "
          "Update the Microsoft.Web.WebView2 NuGet package or set WEBVIEW2_ROOT to a full SDK.")
      endif()
      set(_lib "${WEBVIEW2_FETCH_DIR}/build/native/x64/WebView2LoaderStatic.lib")
    endif()
    set(WEBVIEW2_LIBRARY "${_lib}" CACHE FILEPATH "WebView2 library" FORCE)
    set(_WEBVIEW2_FOUND TRUE)
  endif()
endif()

# -----------------------------------------------------------------------------
# 3. MinGW-GCC: WebView2LoaderStatic.lib is MSVC-built (security cookie / IID
# clashes with WebView2.h). Use the DLL import library when present.
# -----------------------------------------------------------------------------
if(_WEBVIEW2_FOUND AND WEBVIEW2_LIBRARY AND WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
  if(WEBVIEW2_LIBRARY MATCHES "WebView2LoaderStatic\\.lib$")
    string(REPLACE "WebView2LoaderStatic.lib" "WebView2Loader.dll.lib" _wv2_gnu_lib "${WEBVIEW2_LIBRARY}")
    if(EXISTS "${_wv2_gnu_lib}")
      set(WEBVIEW2_LIBRARY "${_wv2_gnu_lib}" CACHE FILEPATH "WebView2 loader (DLL import for MinGW-GCC)" FORCE)
      message(STATUS "WebView2: MinGW-GCC — using ${WEBVIEW2_LIBRARY} instead of static loader")
    endif()
  endif()
endif()

# -----------------------------------------------------------------------------
# 4. Set WEBVIEW2_FOUND
# -----------------------------------------------------------------------------
if(_WEBVIEW2_FOUND AND WEBVIEW2_INCLUDE_DIR AND WEBVIEW2_LIBRARY)
  if(NOT WEBVIEW2_FIND_QUIETLY)
    message(STATUS "Found WebView2: ${WEBVIEW2_INCLUDE_DIR}")
  endif()
  set(WEBVIEW2_FOUND TRUE)
else()
  set(WEBVIEW2_FOUND FALSE)
  if(WEBVIEW2_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find WebView2 SDK. Install it via Visual Studio (C++ WebView2 workload) or NuGet.")
  endif()
endif()
