#!/usr/bin/env bash
# Copies a Grazer tree to a build box over ssh and runs Scripts/measure-build.sh
# there, so macOS and Windows numbers come from the same script as Linux ones.
#
#   Scripts/measure-build-remote.sh <macos|windows> <user@host> <remote-dir> [git-ref]
#
# Without a git ref the working tree is copied (tracked and untracked files,
# minus ignored ones); with one, that commit is. <remote-dir> is replaced.
set -euo pipefail

platform=${1:?usage: measure-build-remote.sh <macos|windows> <user@host> <remote-dir> [git-ref]}
host=${2:?missing user@host}
remote_dir=${3:?missing remote dir}
git_ref=${4:-}
repo_root=$(cd "$(dirname "$0")/.." && pwd)
visual_studio_environment='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'

archive_tree() {
    if [[ -n $git_ref ]]; then
        git -C "$repo_root" archive "$git_ref"
    else
        git -C "$repo_root" ls-files -z --cached --others --exclude-standard \
            | (cd "$repo_root" && tar -cf - --null -T -)
    fi
}

archive_tree | ssh -o BatchMode=yes "$host" \
    "rm -rf $remote_dir && mkdir -p $remote_dir && tar -xf - -C $remote_dir"

# The measure script is always the working tree's, so before and after are counted alike.
ssh -o BatchMode=yes "$host" "cat > $remote_dir/Scripts/measure-build.sh" \
    < "$repo_root/Scripts/measure-build.sh"

case $platform in
    macos)
        ssh -o BatchMode=yes "$host" \
            "zsh -lc 'cd $remote_dir && bash Scripts/measure-build.sh build-measure'"
        ;;
    windows)
        # Ninja with MSVC needs the Visual Studio environment, which only a cmd batch file sets.
        ssh -o BatchMode=yes "$host" "cd $remote_dir && printf '%s\r\n' \
            '@echo off' 'call \"$visual_studio_environment\" >nul' \
            'bash Scripts/measure-build.sh build-measure' > measure-build.bat \
            && cmd //c measure-build.bat"
        ;;
    *)
        echo "unknown platform '$platform': expected macos or windows" >&2
        exit 2
        ;;
esac
