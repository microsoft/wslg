#!/bin/bash
# `-o pipefail` so an in-pipeline `git describe` failure isn't masked by a
# later cat/echo. `-u` is deliberately omitted: a few vendor-version vars
# are intentionally derived with fall-through defaults below.
set -eo pipefail

# Local dev build: derive a clean NuGet-style version plus the full
# commit SHA for the wslg repo, plus version identifiers for each
# vendor component, all of which the Dockerfile bakes into
# /etc/versions.txt. CI uses the same get-nuget-version.sh helper with
# a branch-derived separator (see wslg-build's pipeline-shared.yml).
#
# `|| true` lets shallow/tag-less local checkouts still build (falling
# back to "dev"). Stderr is intentionally NOT silenced so the real
# reason for a fallback (e.g. "make sure tags are available") is visible.
WSLG_VERSION=$(./devops/get-nuget-version.sh "-Beta" || true)
WSLG_VERSION=${WSLG_VERSION:-dev}
# Use the literal "dev" sentinel (not "unknown") for the git-failure fallback
# so the Dockerfile's fail-fast can keep rejecting the truly-suspicious
# "unknown"/"<unknown>" cases that only ever appear when CI forgets a
# --build-arg, while still letting a tarball-sourced local-dev checkout
# build successfully.
WSLG_COMMIT=$(git rev-parse HEAD 2>/dev/null || echo "dev")

# Vendor components: use rev-parse for the git-sourced ones, fall back
# to the "dev" sentinel if a vendor dir was sourced from a tarball locally.
vendor_commit() { git -C "$1" rev-parse HEAD 2>/dev/null || echo "dev"; }
DIRECTX_HEADERS_VERSION=$(vendor_commit vendor/DirectX-Headers-1.0)
FREERDP_COMMIT=$(vendor_commit vendor/FreeRDP)
MESA_VERSION=$(vendor_commit vendor/mesa)
PULSEAUDIO_COMMIT=$(vendor_commit vendor/pulseaudio)
WESTON_COMMIT=$(vendor_commit vendor/weston)

echo "=== Building Docker image (WSLG_VERSION=$WSLG_VERSION WSLG_COMMIT=$WSLG_COMMIT) ==="
docker build -f Dockerfile -t system-distro-x64 . \
    --build-arg WSLG_VERSION="$WSLG_VERSION" \
    --build-arg WSLG_COMMIT="$WSLG_COMMIT" \
    --build-arg WSLG_ARCH=x86_64 \
    --build-arg DIRECTX_HEADERS_VERSION="$DIRECTX_HEADERS_VERSION" \
    --build-arg FREERDP_COMMIT="$FREERDP_COMMIT" \
    --build-arg MESA_VERSION="$MESA_VERSION" \
    --build-arg PULSEAUDIO_COMMIT="$PULSEAUDIO_COMMIT" \
    --build-arg WESTON_COMMIT="$WESTON_COMMIT"

echo ""
echo "=== Exporting Docker container to tar ==="
docker export $(docker create system-distro-x64) > system_x64.tar

echo ""
echo "=== Converting tar to VHD ==="
# Run tar2ext4 directly from GitHub
go run github.com/Microsoft/hcsshim/cmd/tar2ext4@v0.13.0 -vhd -i system_x64.tar -o system_x64.vhd
echo ""
echo "=== Done! ==="
echo "Output file: $(pwd)/system_x64.vhd"
ls -lh system_x64.vhd
