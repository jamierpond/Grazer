# Grazer dev command runner. Every recipe is a thin one-liner delegating to a
# Node script in Scripts/ — the dispatchers (compile.mjs, run.mjs) pick the
# platform variant (Scripts/<cmd>-<macos|windows>.mjs) from process.platform,
# so the same `just` commands work everywhere.

set windows-shell := ["cmd.exe", "/c"]

# Show the recipe list by default.
default:
    @just --list

[doc('Configure (if needed) + build Grazer for the current platform')]
build:
    node Scripts/compile.mjs

[doc('Build + launch Grazer')]
run:
    node Scripts/run.mjs

[macos]
[doc('Build Release + install to /Applications/Grazer.app, registered with LaunchServices')]
install:
    node Scripts/install-macos.mjs

[doc('Delete the build directory')]
clean:
    node Scripts/clean.mjs

[doc('Run the palette UI vite dev server (configure with -DEACP_WEBVIEW_DEV=ON)')]
web-dev:
    node Scripts/web-dev.mjs

[doc('Rebuild Assets/Icon/Icon.png + Icon.ico from Source.png (needs ImageMagick 7)')]
icon:
    bash Scripts/make-icons.sh
