#!/bin/bash
set -e

# Local dev build: derive a clean NuGet-style version plus the full
# commit SHA, both of which the Dockerfile bakes into /etc/versions.txt.
# CI uses the same get-nuget-version.sh helper with a branch-derived
# separator (see wslg-build's pipeline-shared.yml).
WSLG_VERSION=$(./devops/get-nuget-version.sh "-Beta" 2>/dev/null || true)
WSLG_VERSION=${WSLG_VERSION:-dev}
WSLG_COMMIT=$(git rev-parse HEAD 2>/dev/null || echo "unknown")

echo "=== Building Docker image (WSLG_VERSION=$WSLG_VERSION WSLG_COMMIT=$WSLG_COMMIT) ==="
docker build -f Dockerfile -t system-distro-x64 . \
    --build-arg WSLG_VERSION="$WSLG_VERSION" \
    --build-arg WSLG_COMMIT="$WSLG_COMMIT" \
    --build-arg WSLG_ARCH=x86_64

echo ""
echo "=== Exporting Docker container to tar ==="
docker export $(docker create system-distro-x64) > system_x64.tar

echo ""
echo "=== Converting tar to VHD ==="
# Run tar2ext4 directly from GitHub
go run github.com/Microsoft/hcsshim/cmd/tar2ext4@latest -vhd -i system_x64.tar -o system_x64.vhd
echo ""
echo "=== Done! ==="
echo "Output file: $(pwd)/system_x64.vhd"
ls -lh system_x64.vhd
