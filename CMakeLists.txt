cmake_minimum_required(VERSION 3.25)
project(Bamboo VERSION 1.0.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ─── Auto-fetch CEF ───────────────────────────────────────────────────────────
# Detects your OS + arch and downloads the matching CEF binary automatically.
# No manual download needed. Cached in build dir after first run.
include(FetchCEF)

# ─── nlohmann/json ────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(json)

# ─── CEF cmake helpers ────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
include("${CEF_ROOT}/cmake/cef_variables.cmake")
include("${CEF_ROOT}/cmake/cef_macros.cmake")
add_subdirectory(${CEF_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/cef_build EXCLUDE_FROM_ALL)

# ─── bamboo library ───────────────────────────────────────────────────────────
set(BAMBOO_SOURCES src/App.cpp src/Browser.cpp)

if(APPLE)
    list(APPEND BAMBOO_SOURCES src/platform/StyleApplicator_mac.mm)
    set_source_files_properties(src/platform/StyleApplicator_mac.mm
        PROPERTIES COMPILE_FLAGS "-x objective-c++")
elseif(WIN32)
    list(APPEND BAMBOO_SOURCES src/platform/StyleApplicator_win.cpp)
else()
    list(APPEND BAMBOO_SOURCES src/platform/StyleApplicator_linux.cpp)
endif()

add_library(bamboo STATIC ${BAMBOO_SOURCES})

target_include_directories(bamboo PUBLIC
    include
    ${CEF_ROOT}
    ${CEF_ROOT}/include
)

target_link_libraries(bamboo PUBLIC
    libcef_dll_wrapper
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(bamboo PUBLIC "${CEF_ROOT}/Release/libcef.lib" dwmapi uxtheme)
    target_compile_definitions(bamboo PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE)
elseif(APPLE)
    target_link_libraries(bamboo PUBLIC "-framework AppKit" "-framework Foundation" "-framework QuartzCore")
    find_library(CEF_FWK "Chromium Embedded Framework" PATHS "${CEF_ROOT}/Release" NO_DEFAULT_PATH)
    if(CEF_FWK)
        target_link_libraries(bamboo PUBLIC ${CEF_FWK})
    endif()
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    target_include_directories(bamboo PUBLIC ${GTK3_INCLUDE_DIRS})
    target_link_libraries(bamboo PUBLIC "${CEF_ROOT}/Release/libcef.so" ${GTK3_LIBRARIES})
endif()

# ─── Example app ──────────────────────────────────────────────────────────────
add_executable(bamboo_demo examples/main.cpp)
target_link_libraries(bamboo_demo PRIVATE bamboo)

# ─── macOS: build proper .app bundle ─────────────────────────────────────────
if(APPLE)
    include(BambooBundleMacOS)
    bamboo_create_macos_bundle(
        TARGET      bamboo_demo
        BUNDLE_NAME "Bamboo Demo"
        BUNDLE_ID   "com.bamboo.demo"
        VERSION     "1.0.0"
        COPYRIGHT   "© 2025 Bamboo"
        CEF_ROOT    "${CEF_ROOT}"
    )
endif()

# ─── Windows / Linux: copy CEF runtime files next to binary ──────────────────
if(NOT APPLE)
    add_custom_command(TARGET bamboo_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CEF_ROOT}/Release"   $<TARGET_FILE_DIR:bamboo_demo>
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CEF_ROOT}/Resources" $<TARGET_FILE_DIR:bamboo_demo>
    )
endif()

install(TARGETS bamboo ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY cmake/    DESTINATION cmake)
