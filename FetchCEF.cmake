# cmake/FetchCEF.cmake
# Auto-detects platform/arch and downloads the correct CEF binary distribution.
# Sets CEF_ROOT to the extracted directory.

cmake_minimum_required(VERSION 3.25)
include(FetchContent)

# ─── CEF version ──────────────────────────────────────────────────────────────
# Update this to pick up a newer release.
# Full list: https://cef-builds.spotifycdn.com/index.html
set(BAMBOO_CEF_VERSION "122.1.10+gd5f45cc+chromium-122.0.6261.112" CACHE STRING "CEF binary version to download")

# ─── Detect platform and architecture ────────────────────────────────────────
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_CEF_PLATFORM "windows64")
    else()
        set(_CEF_PLATFORM "windows32")
    endif()
    set(_CEF_EXT "zip")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # Detect Apple Silicon vs Intel
    execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE _HOST_ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_HOST_ARCH STREQUAL "arm64" OR CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
        set(_CEF_PLATFORM "macosarm64")
    else()
        set(_CEF_PLATFORM "macosx64")
    endif()
    set(_CEF_EXT "tar.bz2")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE _HOST_ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_HOST_ARCH STREQUAL "aarch64")
        set(_CEF_PLATFORM "linuxarm64")
    elseif(_HOST_ARCH STREQUAL "armv7l")
        set(_CEF_PLATFORM "linuxarm")
    else()
        set(_CEF_PLATFORM "linux64")
    endif()
    set(_CEF_EXT "tar.bz2")
else()
    message(FATAL_ERROR "Bamboo: Unsupported platform '${CMAKE_SYSTEM_NAME}'")
endif()

# Encode the version string for the URL ('+' → '%2B')
string(REPLACE "+" "%2B" _CEF_VERSION_URL "${BAMBOO_CEF_VERSION}")
string(REPLACE "+" "_"   _CEF_VERSION_DIR "${BAMBOO_CEF_VERSION}")

set(_CEF_DIRNAME  "cef_binary_${_CEF_VERSION_DIR}_${_CEF_PLATFORM}")
set(_CEF_FILENAME "${_CEF_DIRNAME}.tar.bz2")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(_CEF_FILENAME "${_CEF_DIRNAME}.zip")
endif()

set(_CEF_URL
    "https://cef-builds.spotifycdn.com/${_CEF_FILENAME}"
)

message(STATUS "Bamboo: Fetching CEF for platform '${_CEF_PLATFORM}'")
message(STATUS "Bamboo: CEF URL: ${_CEF_URL}")

# ─── Download & extract ───────────────────────────────────────────────────────
# FetchContent caches in CMAKE_BINARY_DIR so re-runs are instant.
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
    cef_binary
    URL      "${_CEF_URL}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    # No hash here — Spotify's CDN updates files with the same name for patches.
    # If you want reproducible builds, pin the URL to a specific hash:
    # URL_HASH SHA256=<hash>
)

FetchContent_MakeAvailable(cef_binary)

set(CEF_ROOT "${cef_binary_SOURCE_DIR}" CACHE PATH "Path to CEF binary distribution" FORCE)

message(STATUS "Bamboo: CEF_ROOT = ${CEF_ROOT}")
