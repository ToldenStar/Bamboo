# cmake/BambooBundleMacOS.cmake
# Creates a proper macOS .app bundle for a Bamboo application.
#
# Usage:
#   bamboo_create_macos_bundle(
#       TARGET          MyApp
#       BUNDLE_NAME     "My App"
#       BUNDLE_ID       "com.example.myapp"
#       VERSION         "1.0.0"
#       COPYRIGHT       "© 2025 My Company"
#       ICON            "path/to/icon.icns"   # optional
#       CEF_ROOT        "${CEF_ROOT}"
#   )

function(bamboo_create_macos_bundle)
    cmake_parse_arguments(BUNDLE
        ""
        "TARGET;BUNDLE_NAME;BUNDLE_ID;VERSION;COPYRIGHT;ICON;CEF_ROOT"
        ""
        ${ARGN}
    )

    if(NOT BUNDLE_TARGET)
        message(FATAL_ERROR "bamboo_create_macos_bundle: TARGET is required")
    endif()
    if(NOT BUNDLE_CEF_ROOT)
        message(FATAL_ERROR "bamboo_create_macos_bundle: CEF_ROOT is required")
    endif()

    set(_APP_NAME "${BUNDLE_BUNDLE_NAME}")
    if(NOT _APP_NAME)
        set(_APP_NAME "${BUNDLE_TARGET}")
    endif()

    set(_BUNDLE_ID "${BUNDLE_BUNDLE_ID}")
    if(NOT _BUNDLE_ID)
        set(_BUNDLE_ID "com.bamboo.${BUNDLE_TARGET}")
    endif()

    set(_VERSION "${BUNDLE_VERSION}")
    if(NOT _VERSION)
        set(_VERSION "1.0.0")
    endif()

    set(_COPYRIGHT "${BUNDLE_COPYRIGHT}")
    if(NOT _COPYRIGHT)
        set(_COPYRIGHT "© 2025")
    endif()

    # ── Configure the target as a macOS bundle ────────────────────────────────
    set_target_properties(${BUNDLE_TARGET} PROPERTIES
        MACOSX_BUNDLE                 TRUE
        MACOSX_BUNDLE_BUNDLE_NAME     "${_APP_NAME}"
        MACOSX_BUNDLE_BUNDLE_VERSION  "${_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER  "${_BUNDLE_ID}"
        MACOSX_BUNDLE_COPYRIGHT       "${_COPYRIGHT}"
        MACOSX_BUNDLE_INFO_PLIST      "${CMAKE_CURRENT_BINARY_DIR}/${BUNDLE_TARGET}_Info.plist"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${_BUNDLE_ID}"
    )

    # ── Generate Info.plist ───────────────────────────────────────────────────
    set(_ICON_FILE "")
    if(BUNDLE_ICON)
        get_filename_component(_ICON_FILE "${BUNDLE_ICON}" NAME)
    endif()

    configure_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Info.plist.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${BUNDLE_TARGET}_Info.plist"
        @ONLY
    )

    # ── Bundle paths ──────────────────────────────────────────────────────────
    set(_BUNDLE_DIR "$<TARGET_FILE_DIR:${BUNDLE_TARGET}>/../..")
    set(_MACOS_DIR  "$<TARGET_FILE_DIR:${BUNDLE_TARGET}>")
    set(_RES_DIR    "${_BUNDLE_DIR}/Resources")
    set(_FWK_DIR    "${_BUNDLE_DIR}/Frameworks")

    set(_CEF_FWK_SRC "${BUNDLE_CEF_ROOT}/Release/Chromium Embedded Framework.framework")

    # ── Post-build: Copy CEF framework + helper apps ──────────────────────────
    add_custom_command(TARGET ${BUNDLE_TARGET} POST_BUILD
        COMMENT "Assembling ${_APP_NAME}.app bundle..."

        # 1. Copy CEF framework into Frameworks/
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_FWK_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${_CEF_FWK_SRC}"
            "${_FWK_DIR}/Chromium Embedded Framework.framework"

        # 2. Copy locale/resource files
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_RES_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${BUNDLE_CEF_ROOT}/Resources"
            "${_RES_DIR}"

        # 3. Create the helper app bundle (required by CEF for sandboxed sub-processes)
        COMMAND ${CMAKE_COMMAND}
            -DHELPER_TARGET=${BUNDLE_TARGET}
            -DHELPER_APP_NAME=${_APP_NAME}
            -DHELPER_BUNDLE_ID=${_BUNDLE_ID}
            -DHELPER_MACOS_DIR=${_MACOS_DIR}
            -DHELPER_FWK_DIR=${_FWK_DIR}
            -DCEF_ROOT=${BUNDLE_CEF_ROOT}
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CreateHelpers.cmake"

        # 4. Copy icon if provided
        COMMAND ${CMAKE_COMMAND} -E $<IF:$<BOOL:${BUNDLE_ICON}>,copy,true>
            ${BUNDLE_ICON}
            "${_RES_DIR}/${_ICON_FILE}"
    )

    # ── Code signing (ad-hoc for local builds) ────────────────────────────────
    add_custom_command(TARGET ${BUNDLE_TARGET} POST_BUILD
        COMMENT "Ad-hoc signing ${_APP_NAME}.app..."
        COMMAND codesign --force --deep --sign - "${_BUNDLE_DIR}"
        VERBATIM
    )

endfunction()
