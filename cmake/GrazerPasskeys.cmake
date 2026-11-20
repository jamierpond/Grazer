# Passkeys (WebAuthn) for arbitrary sites.
#
# Grazer is its own passkey authenticator: it keeps each passkey's private key
# in the Secure Enclave, in the keychain access group <team>.<bundle id>. The
# keychain only lets an app use an access group its signature claims, and the
# claim only holds when a provisioning profile embedded in the bundle allows it,
# so this signs Grazer with a real identity, the entitlements in
# Grazer.entitlements.in and the profile at Contents/embedded.provisionprofile.
#
#   GRAZER_TEAM_ID=<team id> \
#   GRAZER_CODESIGN_IDENTITY="Developer ID Application: <name> (<team id>)" \
#   GRAZER_PROVISIONING_PROFILE=/path/to/Grazer_Developer_ID.provisionprofile \
#       just install
#
# scripts/install-macos.mjs turns those into -DGRAZER_PASSKEYS=ON and the three
# cache values below, and configures OFF when none of them is set.
#
# The team and identity have no defaults: which developer account signs
# Grazer is always an explicit choice.
#
# Apple's managed browser entitlement
# com.apple.developer.web-browser.public-key-credential is claimed only when
# the profile carries it: claiming an entitlement the profile does not allow
# gets the app killed at launch.

option(GRAZER_PASSKEYS
        "Sign Grazer with a team, identity and provisioning profile so it can keep passkeys in the Secure Enclave"
        OFF)

set(GRAZER_TEAM_ID "" CACHE STRING "Apple team that owns Grazer's App ID")
set(GRAZER_CODESIGN_IDENTITY "" CACHE STRING
        "codesign identity matching the provisioning profile")
set(GRAZER_PROVISIONING_PROFILE "" CACHE FILEPATH
        "Developer ID provisioning profile for Grazer's bundle id")

set(GRAZER_BROWSER_PASSKEY_ENTITLEMENT com.apple.developer.web-browser.public-key-credential)

function(_grazer_profile_value decoded key out)
    execute_process(
            COMMAND /usr/libexec/PlistBuddy -c "Print :${key}" "${decoded}"
            OUTPUT_VARIABLE value
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

function(_grazer_decode_profile profile out)
    if (NOT EXISTS "${profile}")
        message(FATAL_ERROR
                "GRAZER_PASSKEYS needs GRAZER_PROVISIONING_PROFILE: a Developer ID "
                "provisioning profile for ${GRAZER_TEAM_ID}.${GRAZER_BUNDLE_ID} "
                "(got '${profile}').")
    endif ()

    set(decoded "${CMAKE_CURRENT_BINARY_DIR}/GrazerProfile.plist")
    execute_process(
            COMMAND security cms -D -i "${profile}" -o "${decoded}"
            RESULT_VARIABLE result
            ERROR_QUIET)

    if (NOT result EQUAL 0)
        message(FATAL_ERROR "Can't decode provisioning profile ${profile}")
    endif ()

    set(${out} "${decoded}" PARENT_SCOPE)
endfunction()

# The profile allows a keychain group either by name or by a "<team>.*" pattern.
function(_grazer_check_keychain_group decoded profile)
    _grazer_profile_value("${decoded}" "Entitlements:keychain-access-groups" groups)
    set(group "${GRAZER_TEAM_ID}.${GRAZER_BUNDLE_ID}")
    string(FIND "${groups}" "${group}" exact_match)
    string(FIND "${groups}" "${GRAZER_TEAM_ID}.*" pattern_match)

    if (exact_match EQUAL -1 AND pattern_match EQUAL -1)
        message(FATAL_ERROR
                "${profile} doesn't allow the keychain access group ${group}, "
                "where Grazer keeps its passkeys; it allows: ${groups}")
    endif ()
endfunction()

# Fails at configure time, with the reason, rather than shipping an app that
# AMFI kills on launch. Sets GRAZER_BROWSER_PASSKEY_ENTITLEMENT_LINES to the
# plist lines claiming Apple's browser entitlement, or to nothing when the
# profile doesn't carry it.
function(_grazer_check_profile profile)
    _grazer_decode_profile("${profile}" decoded)
    _grazer_profile_value("${decoded}" "Entitlements:com.apple.application-identifier" app_id)

    if (NOT app_id STREQUAL "${GRAZER_TEAM_ID}.${GRAZER_BUNDLE_ID}")
        message(FATAL_ERROR
                "${profile} is for '${app_id}', not ${GRAZER_TEAM_ID}.${GRAZER_BUNDLE_ID}")
    endif ()

    _grazer_check_keychain_group("${decoded}" "${profile}")
    _grazer_profile_value("${decoded}" "Entitlements:${GRAZER_BROWSER_PASSKEY_ENTITLEMENT}" browser)

    if (browser STREQUAL "true")
        message(STATUS "Grazer: ${profile} carries ${GRAZER_BROWSER_PASSKEY_ENTITLEMENT}; claiming it")
        set(GRAZER_BROWSER_PASSKEY_ENTITLEMENT_LINES
                "<key>${GRAZER_BROWSER_PASSKEY_ENTITLEMENT}</key>\n\t<true/>" PARENT_SCOPE)
    else ()
        message(STATUS "Grazer: ${profile} doesn't carry ${GRAZER_BROWSER_PASSKEY_ENTITLEMENT}; not claiming it")
        set(GRAZER_BROWSER_PASSKEY_ENTITLEMENT_LINES "" PARENT_SCOPE)
    endif ()
endfunction()

function(grazer_enable_passkeys target bundle_id)
    if (NOT APPLE OR NOT GRAZER_PASSKEYS)
        return()
    endif ()

    if (GRAZER_TEAM_ID STREQUAL "" OR GRAZER_CODESIGN_IDENTITY STREQUAL "")
        message(FATAL_ERROR
                "GRAZER_PASSKEYS needs GRAZER_TEAM_ID and GRAZER_CODESIGN_IDENTITY "
                "set explicitly (no defaults).")
    endif ()

    set(GRAZER_BUNDLE_ID "${bundle_id}")
    _grazer_check_profile("${GRAZER_PROVISIONING_PROFILE}")

    set(entitlements "${CMAKE_CURRENT_BINARY_DIR}/${target}.entitlements")
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Grazer.entitlements.in"
            "${entitlements}" @ONLY)

    # Re-sign the whole bundle after the linker's ad-hoc signature: the profile
    # has to be in place first because the signature seals it.
    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy "${GRAZER_PROVISIONING_PROFILE}"
                    "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/embedded.provisionprofile"
            COMMAND codesign --force --timestamp=none
                    --sign "${GRAZER_CODESIGN_IDENTITY}"
                    --entitlements "${entitlements}"
                    "$<TARGET_BUNDLE_DIR:${target}>"
            COMMENT "Signing ${target} with team ${GRAZER_TEAM_ID} and its provisioning profile"
            VERBATIM)
endfunction()
