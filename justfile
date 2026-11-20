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

[doc("Build everything, then run clang-tidy (eacp's checks) over Grazer's sources; fails on any finding")]
[unix]
tidy: build
    cmake --build build
    scripts/tidy.sh build

[doc('Build + launch Grazer')]
run:
    {{msvc}} node scripts/run.mjs

[doc('For now installs the line-coverage build (just install-without-coverage for the normal one): Release (locked deps) into ~/.local/libexec/grazer/Grazer.app (+ ~/.local/bin/grazer), keeping the old and new programs for coverage-report. --keep-running leaves a running Grazer up')]
[macos]
install *args: coverage-build
    {{line_coverage}} keep
    node scripts/install.mjs --line-coverage {{args}}
    {{line_coverage}} keep

[doc('Build Release (locked deps) + install to %LOCALAPPDATA%\Programs\Grazer. --keep-running leaves a running Grazer up')]
[windows]
install *args:
    {{msvc}} node scripts/install.mjs {{args}}

[doc('Build Release (locked deps) without line coverage + install: ~/.local/libexec/grazer/Grazer.app (+ ~/.local/bin/grazer), or %LOCALAPPDATA%\Programs\Grazer on Windows. --keep-running leaves a running Grazer up')]
install-without-coverage *args:
    {{msvc}} node scripts/install.mjs {{args}}

[doc('List the crash reports nobody has marked fixed here and on other machines (list [HOST ...]), or mark one fixed (mark-fixed HOST ID); reads the script from build-test, which just test configures')]
crash-reports *arguments:
    python3 build-test/PondCrashReports/crash-reports.py --app Grazer {{arguments}}

[doc('Move branch-pinned deps (eacp@jp/develop, PondDev@main, Rudder@main, ...) to their latest commits in lockstep.lock; builds nothing')]
update-deps:
    {{msvc}} node scripts/update-deps.mjs

[doc('Delete the build directory')]
clean:
    node scripts/clean.mjs

[doc('Rebuild Assets/Icon/Icon.png + Icon.ico from Source.png (needs ImageMagick 7)')]
icon:
    bash scripts/make-icons.sh

[doc('clang-format every tracked source in place')]
[unix]
format:
    git ls-files '*.h' '*.cpp' '*.mm' | xargs clang-format -i

# ---- Line coverage: which lines of Grazer's own code a normal day runs ----

line_coverage := "python3 build-coverage/PondLineCoverage/line-coverage.py --app Grazer --program Grazer --installed-in ~/.local/libexec/grazer/Grazer.app/Contents/MacOS --own-code-folder Source --own-code-folder Lib --not-own-code-folder Tests"

[doc('Build Release Grazer with line-coverage counters, in build-coverage')]
[macos]
coverage-build:
    node scripts/coverage-build.mjs

[doc('Print the code that never ran in any installed coverage build, largest first, and write an HTML per-line view')]
[macos]
coverage-report *arguments:
    {{line_coverage}} report {{arguments}}

[doc('Delete the line-coverage counts and kept programs')]
[macos]
coverage-reset *arguments:
    {{line_coverage}} reset {{arguments}}
