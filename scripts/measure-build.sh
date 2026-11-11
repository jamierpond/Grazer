#!/usr/bin/env bash
# Counts the compile and link steps of each build Grazer has, then times a clean
# app build, a rebuild with nothing changed and a rebuild after touching one file.
#
#   scripts/measure-build.sh <build-dir> [source-dir] [-- extra cmake args]
#
# Every count comes from a fresh configure, so <build-dir> is deleted first.
set -euo pipefail

build_dir=${1:?usage: measure-build.sh <build-dir> [source-dir] [-- cmake args]}
shift
source_dir=$(cd "$(dirname "$0")/.." && pwd)
if [[ $# -gt 0 && $1 != -- ]]; then
    source_dir=$1
    shift
fi
[[ ${1:-} == -- ]] && shift
extra_cmake_args=("$@")

parallel_jobs=${MEASURE_JOBS:-6}
app_target=${MEASURE_APP_TARGET:-Grazer}
test_target=GrazerTests
touched_file=$source_dir/Lib/Browser/Url.cpp

# A dry run (ninja -n) stops at "Re-running CMake" wherever CONFIGURE_DEPENDS globs
# exist (eacp's web build on macOS and Windows), so list the commands instead.
count_steps() {
    ninja -C "$build_dir" -t commands "$@" | grep -c .
}

count_steps_run_by_build() {
    ninja -C "$build_dir" -j "$parallel_jobs" "$@" | grep '^\[' | grep -vc 'Re-checking globbed directories' || true
}

seconds_since() {
    echo $(( $(date +%s) - $1 ))
}

rm -rf "$build_dir"
cmake -G Ninja -B "$build_dir" -S "$source_dir" -DCMAKE_BUILD_TYPE=Debug \
    ${extra_cmake_args[@]+"${extra_cmake_args[@]}"} > "$build_dir.configure.log" 2>&1 \
    || { tail -20 "$build_dir.configure.log"; exit 1; }

default_target=$app_target
if ninja -C "$build_dir" -t query "$app_target" > /dev/null 2>&1; then
    echo "app steps ($app_target):      $(count_steps "$app_target")"
else
    default_target=$test_target
    echo "app steps:                $app_target is not a target on this platform"
fi
echo "bare cmake --build steps: $(count_steps all)"
echo "test steps ($test_target): $(count_steps "$test_target")"

start=$(date +%s)
ninja -C "$build_dir" -j "$parallel_jobs" "$default_target" > /dev/null
echo "clean build of $default_target: $(seconds_since "$start")s"

echo "no-change rebuild steps:  $(count_steps_run_by_build "$default_target")"

touch "$touched_file"
start=$(date +%s)
ninja -C "$build_dir" -j "$parallel_jobs" "$default_target" > /dev/null
echo "one-file rebuild ($(basename "$touched_file")): $(seconds_since "$start")s"

ninja -C "$build_dir" -j "$parallel_jobs" "$test_target" > /dev/null
test_binary=$(find "$build_dir/Tests" -maxdepth 2 -name "$test_target" -o -maxdepth 2 -name "$test_target.exe" | head -1)
"$test_binary" > "$build_dir.tests.log" 2>&1 \
    && echo "tests: pass ($(tail -1 "$build_dir.tests.log"))" \
    || { tail -20 "$build_dir.tests.log"; echo "tests: FAIL"; exit 1; }

# An unknown flag makes Grazer print a usage error and exit 2 before any window opens.
expected_usage_error_exit_code=2
if [[ $default_target == "$app_target" ]]; then
    app_binary=$(find "$build_dir" -maxdepth 4 -path '*/MacOS/Grazer' -o -maxdepth 4 -name Grazer.exe | head -1)
    set +e
    "$app_binary" --measure-build-smoke-check > "$build_dir.smoke.log" 2>&1
    app_exit_code=$?
    set -e
    echo "app smoke check: exit $app_exit_code (expect $expected_usage_error_exit_code): $(head -1 "$build_dir.smoke.log")"
    [[ $app_exit_code == "$expected_usage_error_exit_code" ]]
fi
