#!/bin/bash
# Bash equivalent of devops/version_functions.ps1::Get-NugetVersion.
#
# Parses `git describe --tags --match *.*.* --exclude *.*.*.* --abbrev=1`
# against the wslg checkout in $PWD and prints the Microsoft.WSLg NuGet
# package version using the supplied separator:
#   "."       - release/* branches:  v1.0.73 + 1 commit  -> 1.0.73.1
#   anything  - main/feature/tags:   v1.0.77 + 3 commits, "-Beta" -> 1.0.78-Beta3
# When HEAD is exactly on a tag, the base version is returned regardless of
# separator (e.g. "1.0.77"). The two implementations (this script and the
# PowerShell version_functions.ps1) MUST stay in sync; if you touch one,
# update the other.

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "usage: $0 <separator>" >&2
    exit 2
fi
separator="$1"

describe="$(git describe --tags --match '*.*.*' --exclude '*.*.*.*' --abbrev=1)"
base_tag="${describe%%-*}"
base="${base_tag#v}"

if ! [[ "$base" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "ERROR: git describe output '$describe' does not start with a Major.Minor.Patch tag (parsed base: '$base')." >&2
    exit 1
fi

if [ "$describe" = "$base_tag" ]; then
    revision=0
else
    rest="${describe#${base_tag}-}"
    revision="${rest%%-*}"
fi

if [ "$revision" = "0" ]; then
    echo "$base"
elif [ "$separator" = "." ]; then
    echo "$base.$revision"
else
    major="${base%%.*}"
    rest="${base#*.}"
    minor="${rest%%.*}"
    patch="${rest#*.}"
    echo "$major.$minor.$((patch + 1))$separator$revision"
fi
