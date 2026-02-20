# cmake/CreateHelpers.cmake
# Creates the CEF helper app bundles required by macOS sandboxing.
# Called as a POST_BUILD script from BambooBundleMacOS.cmake.
#
# CEF requires several helper .app bundles inside the main bundle:
#   MyApp Helper.app
#   MyApp Helper (GPU).app
#   MyApp Helper (Plugin).app
#   MyApp Helper (Renderer).app
#
# Each must have its own Info.plist and a symlink to the CEF framework.

foreach(_HELPER_SUFFIX "" " (GPU)" " (Plugin)" " (Renderer)")
    set(_HELPER_APP_DIR
        "${HELPER_MACOS_DIR}/${HELPER_APP_NAME} Helper${_HELPER_SUFFIX}.app"
    )
    set(_HELPER_CONTENTS_DIR "${_HELPER_APP_DIR}/Contents")
    set(_HELPER_MACOS_SUB    "${_HELPER_CONTENTS_DIR}/MacOS")
    set(_HELPER_FWK_LINK     "${_HELPER_CONTENTS_DIR}/Frameworks")

    file(MAKE_DIRECTORY "${_HELPER_MACOS_SUB}")

    # Each helper executable must exist — copy the main binary as the helper binary.
    # In a real distribution you'd build a separate thin helper target; for
    # development this is sufficient since CEF dispatches by process type.
    file(COPY_FILE
        "${HELPER_MACOS_DIR}/${HELPER_TARGET}"
        "${_HELPER_MACOS_SUB}/${HELPER_APP_NAME} Helper${_HELPER_SUFFIX}"
    )

    # Create a relative symlink back to the shared CEF framework
    file(CREATE_LINK
        "../../Frameworks"
        "${_HELPER_FWK_LINK}"
        SYMBOLIC
    )

    # Generate Info.plist for the helper
    set(_HELPER_BUNDLE_ID "${HELPER_BUNDLE_ID}.helper")
    if(NOT _HELPER_SUFFIX STREQUAL "")
        string(REPLACE " " "" _SUFFIX_KEY "${_HELPER_SUFFIX}")
        string(REPLACE "(" "" _SUFFIX_KEY "${_SUFFIX_KEY}")
        string(REPLACE ")" "" _SUFFIX_KEY "${_SUFFIX_KEY}")
        set(_HELPER_BUNDLE_ID "${HELPER_BUNDLE_ID}.helper${_SUFFIX_KEY}")
    endif()

    set(_HELPER_PLIST_CONTENT
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\"
    \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">
<plist version=\"1.0\">
<dict>
    <key>CFBundleDevelopmentRegion</key><string>en</string>
    <key>CFBundleDisplayName</key><string>${HELPER_APP_NAME} Helper${_HELPER_SUFFIX}</string>
    <key>CFBundleExecutable</key><string>${HELPER_APP_NAME} Helper${_HELPER_SUFFIX}</string>
    <key>CFBundleIdentifier</key><string>${_HELPER_BUNDLE_ID}</string>
    <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
    <key>CFBundleName</key><string>${HELPER_APP_NAME} Helper${_HELPER_SUFFIX}</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>1.0</string>
    <key>CFBundleVersion</key><string>1</string>
    <key>LSMinimumSystemVersion</key><string>10.13</string>
    <key>com.apple.security.cs.allow-jit</key><true/>
    <key>com.apple.security.cs.allow-unsigned-executable-memory</key><true/>
    <key>com.apple.security.cs.disable-library-validation</key><true/>
</dict>
</plist>")

    file(WRITE "${_HELPER_CONTENTS_DIR}/Info.plist" "${_HELPER_PLIST_CONTENT}")

endforeach()
