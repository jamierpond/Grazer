# Passkeys (WebAuthn) for arbitrary sites.
#
# WKWebView hands navigator.credentials.create/get to AuthenticationServices,
# which refuses any app without a team application identifier and, for sites
# that aren't the app's own associated domains, without Apple's managed
# browser entitlement com.apple.developer.web-browser.public-key-credential.
# Both have to be authorised by a provisioning profile embedded in the bundle,
# so this signs Grazer with a real identity, the entitlements in
# Grazer.entitlements.in and the profile at Contents/embedded.provisionprofile.
#
#   cmake -B build -DGRAZER_PASSKEYS=ON \
#       -DGRAZER_PROVISIONING_PROFILE=/path/to/Grazer_Developer_ID.provisionprofile
#
# Off by default: without a profile carrying the entitlement, a signed build
# claiming it would be killed at launch, and the ad-hoc build is what we have.

option(GRAZER_PASSKEYS "Sign Grazer with the browser passkey entitlement + profile" OFF)

set(GRAZER_TEAM_ID "MBHR5VAUVQ" CACHE STRING "Apple team that owns Grazer's App ID")
set(GRAZER_CODESIGN_IDENTITY "Developer ID Application: Tamber, Incorporated (MBHR5VAUVQ)"
        CACHE STRING "codesign identity matching the provisioning profile")
set(GRAZER_PROVISIONING_PROFILE "" CACHE FILEPATH
        "Developer ID provisioning profile for Grazer's bundle id with the web-browser.public-key-credential capability")

set(GRAZER_PASSKEY_ENTITLEMENT com.apple.developer.web-browser.public-key-credential)

function(_grazer_profile_value decoded key out)
    execute_process(
            COMMAND /usr/libexec/PlistBuddy -c "Print :${key}" "${decoded}"
            OUTPUT_VARIABLE value
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

# Fail at configure time, with the reason, rather than shipping an app that
# AMFI kills on launch or that still can't do passkeys.
function(_grazer_check_profile profile bundle_id)
    if (NOT EXISTS "${profile}")
        message(FATAL_ERROR
                "GRAZER_PASSKEYS needs GRAZER_PROVISIONING_PROFILE: a Developer ID "
                "provisioning profile for ${GRAZER_TEAM_ID}.${bundle_id} with the "
                "${GRAZER_PASSKEY_ENTITLEMENT} capability (got '${profile}').")
    endif ()

    set(decoded "${CMAKE_CURRENT_BINARY_DIR}/GrazerProfile.plist")
    execute_process(
            COMMAND security cms -D -i "${profile}" -o "${decoded}"
            RESULT_VARIABLE result
            ERROR_QUIET)

    if (NOT result EQUAL 0)
        message(FATAL_ERROR "Can't decode provisioning profile ${profile}")
    endif ()

    _grazer_profile_value("${decoded}" "Entitlements:com.apple.application-identifier" app_id)
    _grazer_profile_value("${decoded}" "Entitlements:${GRAZER_PASSKEY_ENTITLEMENT}" passkeys)

    if (NOT app_id STREQUAL "${GRAZER_TEAM_ID}.${bundle_id}")
        message(FATAL_ERROR
                "${profile} is for '${app_id}', not ${GRAZER_TEAM_ID}.${bundle_id}")
    endif ()

    if (NOT passkeys STREQUAL "true")
        message(FATAL_ERROR
                "${profile} doesn't carry ${GRAZER_PASSKEY_ENTITLEMENT}. Apple has "
                "to grant it to team ${GRAZER_TEAM_ID} first (developer.apple.com/"
                "contact/request/macos-browsers-passkeys), then enable it on the "
                "App ID and regenerate the profile.")
    endif ()
endfunction()

function(grazer_enable_passkeys target bundle_id)
    if (NOT APPLE OR NOT GRAZER_PASSKEYS)
        return()
    endif ()

    _grazer_check_profile("${GRAZER_PROVISIONING_PROFILE}" "${bundle_id}")

    set(GRAZER_BUNDLE_ID "${bundle_id}")
    set(entitlements "${CMAKE_CURRENT_BINARY_DIR}/${target}.entitlements")
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Grazer.entitlements.in"
            "${entitlements}" @ONLY)

    # The browser entitlement's review criteria want the app registered for
    # http/https.
    set_target_properties(${target} PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/GrazerInfo.plist.in")

    # Re-sign the whole bundle after the linker's ad-hoc signature: the profile
    # has to be in place first because the signature seals it.
    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy "${GRAZER_PROVISIONING_PROFILE}"
                    "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/embedded.provisionprofile"
            COMMAND codesign --force --timestamp=none
                    --sign "${GRAZER_CODESIGN_IDENTITY}"
                    --entitlements "${entitlements}"
                    "$<TARGET_BUNDLE_DIR:${target}>"
            COMMENT "Signing ${target} with the passkey entitlement"
            VERBATIM)
endfunction()
