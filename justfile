# Grazer dev commands. Each recipe hands off to scripts/<recipe>.mjs, which
# picks the macOS or Windows variant (scripts/<recipe>-<macos|windows>.mjs).

set windows-shell := ["cmd.exe", "/c"]

# Ninja drives cl directly on Windows, so it needs the Visual Studio environment.
msvc := if os() == "windows" { "powershell -NoProfile -ExecutionPolicy Bypass -File scripts/with-msvc.ps1" } else { "" }

# Show the recipe list by default.
default:
    @just --list

[doc('Configure (if needed) + build Grazer for the current platform')]
build:
    {{msvc}} node scripts/build.mjs

[doc('Build + run the unit tests in build-test/ (leaves build/ alone)')]
test:
    {{msvc}} node scripts/test.mjs

[doc('Build + launch Grazer')]
run:
    {{msvc}} node scripts/run.mjs

[doc('Build Release (locked deps) + install: ~/.local/libexec/grazer/Grazer.app (+ ~/.local/bin/grazer), or %LOCALAPPDATA%\Programs\Grazer on Windows')]
install:
    {{msvc}} node scripts/install.mjs

[doc('Move branch-pinned deps (eacp@grazer, Rudder@main, ...) to their latest commits in lockstep.lock; builds nothing')]
update-deps:
    {{msvc}} node scripts/update-deps.mjs

[doc('Delete the build directory')]
clean:
    node scripts/clean.mjs

[doc('Run the palette UI vite dev server (configure with -DEACP_WEBVIEW_DEV=ON)')]
web-dev:
    node scripts/web-dev.mjs

[doc('Rebuild Assets/Icon/Icon.png + Icon.ico from Source.png (needs ImageMagick 7)')]
icon:
    bash scripts/make-icons.sh
