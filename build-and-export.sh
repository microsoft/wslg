#!/bin/bash
set -e

echo "=== Building Docker image ==="
docker build -f Dockerfile -t system-distro-x64 . \
    --build-arg WSLG_VERSION=1.0.72-test \
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
