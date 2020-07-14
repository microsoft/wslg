# Create a builder image with the compilers, etc. needed
FROM ubuntu:20.04 AS build-env

# Install all the required packages for building. This list is probably
# longer than necessary.
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install --no-install-recommends -y \
    build-essential \
    cmake \
    git \
    libcairo2-dev \
    libcolord-dev \
    libdbus-glib-1-dev \
    libdrm-dev \
    libffi-dev \
    libgbm-dev \
    libgles2-mesa-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstreamer1.0-dev \
    libinput-dev \
    libjpeg-dev \
    liblcms2-dev \
    libpam-dev \
    libpango1.0-dev \
    libpixman-1-dev \
    libssl-dev \
    libsystemd-dev \
    libtool \
    libudev-dev \
    libudev-dev \
    libusb-1.0-0-dev \
    libva-dev \
    libwayland-dev \
    libwebp-dev \
    libx11-dev \
    libx11-xcb-dev \
    libxcb-composite0-dev \
    libxcb-xkb-dev \
    libxcursor-dev \
    libxdamage-dev \
    libxext-dev \
    libxfixes-dev \
    libxi-dev \
    libxinerama-dev \
    libxkbcommon-dev \
    libxkbfile-dev \
    libxml2-dev \
    libxrandr-dev \
    libxrender-dev \
    libxtst-dev \
    libxv-dev \
    lsb-release \
    meson \
    ninja-build \
    pkg-config \
    software-properties-common \
    squashfs-tools \
    uuid-dev \
    wayland-protocols \
    wget

WORKDIR /work
CMD /bin/bash

# Create an image with builds of FreeRDP and Weston
FROM build-env AS dev

ENV prefix=/usr/local/weston
ENV PKG_CONFIG_PATH=${prefix}/lib/pkgconfig:${prefix}/lib/x86_64-linux-gnu/pkgconfig:${prefix}/share/pkgconfig

# Build FreeRDP
COPY vendor/FreeRDP /work/vendor/FreeRDP
WORKDIR /work/vendor/FreeRDP
RUN cmake -G Ninja \
        -B build \
        -DCMAKE_INSTALL_PREFIX=${prefix} \
        -DCMAKE_INSTALL_LIBDIR=${prefix}/lib \
        -DCMAKE_BUILD_TYPE=Release \
        -DWITH_SERVER=ON && \
    ninja -C build -j8 && \
    ninja -C build install

# Build Weston
COPY vendor/weston /work/vendor/weston
WORKDIR /work/vendor/weston
RUN meson --prefix=${prefix} build -Dpipewire=false && \
    ninja -C build -j8 && \
    ninja -C build install

# Create the distro image with just what's needed at runtime.
FROM ubuntu:20.04 as runtime

# Install the packages needed to run weston, freerdp, and xwayland.
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install --no-install-recommends -y \
    libcairo2 \
    libegl1 \
    libinput10 \
    libjpeg8 \
    libpango-1.0.0 \
    libpangocairo-1.0.0 \
    libssl1.1 \
    libwayland-client0 \
    libwayland-cursor0 \
    libwayland-server0 \
    libwebp6 \
    libxcb-composite0-dev \
    libxcursor1 \
    libxkbcommon0 \
    tzdata \
    xinit \
    xwayland

# Install packages to aid in development.
# TODO: these should not be included when building the retail image.
RUN apt-get update && apt-get install --no-install-recommends -y \
    gdb \
    nano \
    vim

# Setup the container environment variable state.
ENV weston_path=/usr/local/weston
ENV XDG_RUNTIME_DIR=/tmp/xdg-runtime-dir
ENV WAYLAND_DISPLAY=wayland-0
ENV LD_LIBRARY_PATH=${weston_path}/lib:${weston_path}/lib/x86_64-linux-gnu
ENV PATH="${weston_path}/bin:${PATH}"
ENV DISPLAY=:0

# Make sure the directories for Wayland and X11 sockets are present.
RUN mkdir "${XDG_RUNTIME_DIR}" && chmod 0700 "${XDG_RUNTIME_DIR}"
RUN mkdir /tmp/.X11-unix && chmod 777 /tmp/.X11-unix

# Copy the built artifacts from the build stage.
COPY --from=dev ${weston_path} ${weston_path}

# start weston with RDP.
#
# --backend=rdp-backend.so : this enables RDP server in weston.
# --port=3391 : port to listen RDP connection (default is 3389)
# --xwayland : enable X11 app support in weston.
#
EXPOSE 3391/tcp
CMD weston --backend=rdp-backend.so --port=3391 --xwayland
