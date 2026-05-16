#!/bin/bash
# Wrapper that conditionally strips debug info for a component, called from
# the Dockerfile to consolidate what used to be 4 copy-pasted RUN blocks
# (one per built component). When SYSTEMDISTRO_DEBUG_BUILD is set the
# strip is skipped so the resulting binaries keep their debug symbols.
#
# Usage: strip_debuginfo.sh <label> <list-path> [build-root]
#   label      : Component name for the log line, e.g. 'FreeRDP'.
#   list-path  : Absolute path to the .list file enumerating binaries to
#                split (one relative path per line, e.g.
#                /work/debuginfo/FreeRDP3.list).
#   build-root : Defaults to /work/build. Passed through as $2 to
#                gen_debuginfo.sh.
set -euo pipefail

if [ $# -lt 2 ]; then
    echo "usage: $0 <label> <list-path> [build-root]" >&2
    exit 2
fi

label="$1"
list="$2"
buildroot="${3:-/work/build}"

if [ -n "${SYSTEMDISTRO_DEBUG_BUILD:-}" ]; then
    echo "== Skip debug strip: $label (SYSTEMDISTRO_DEBUG_BUILD set) =="
    exit 0
fi

echo "== Strip debug info: $label =="
exec /work/debuginfo/gen_debuginfo.sh "$list" "$buildroot"
