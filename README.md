# Introduction

This repository contains a Dockerfile and supporting tools to build the WSL GUI system distro image.

## Quick start

0. Install and start Docker in a Linux or WSL2 environment.

1. Clone the FreeRDP and Weston repositories and checkout the "working" branch from each:

    ```bash
    git clone https://microsoft.visualstudio.com/DefaultCollection/DxgkLinux/_git/FreeRDP vendor/FreeRDP -b working

    git clone https://microsoft.visualstudio.com/DefaultCollection/DxgkLinux/_git/weston vendor/weston -b working
    ```

2. Build the image:

    ```bash
    docker build -t wsl-system-distro .
    ```

    This builds a container image called "wsl-system-distro" that will run weston when it starts.

3. Run the image, allowing port 3391 out and bind mounting the Unix sockets:

    ```bash
    docker run -it --rm -v /tmp/.X11-unix:/tmp/.X11-unix -v /tmp/xdg-runtime-dir:/tmp/xdg-runtime-dir wsl-system-distro
    ```

4. Start mstsc:

    ```bash
    mstsc.exe /v:localhost:3391 rail-weston.rdp
    ```

5. In another terminal, set the environment appropraitely and run apps:

    ```bash
    export DISPLAY=:0
    export WAYLAND_DISPLAY=/tmp/xdg-runtime-dir/wayland-0
    gimp
    ```

6. Make changes to vendor/FreeRDP or vendor/weston and repeat steps 2 through 5.

## Advanced

By default, `docker build` only saves the runtime image. Internally, there is
also a build environment with all the packages needed to build Weston and
FreeRDP, and there is a development environment that has Weston and FreeRDP
built but also includes all the development packages. You can get access to
these via the `--target` option to `docker build`.

For example, to just get a build environment and to run it with the source mapped in instead of copied:

```
docker build --target build-env -t wsl-weston-build-env .
docker run -it --rm -v $PWD/vendor /work/vendor wsl-weston-build-env

# inside the docker container
cd vendor/weston
meson --prefix=/usr/local/weston build -Dpipewire=false
ninja -C build
```

## TODO

* Show how to get a distro image
* Show how to build a squashfs
