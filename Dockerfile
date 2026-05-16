# Base image for both the builder and the runtime stages. Override at build
# time with --build-arg MARINER_IMAGE=... to track a different Azure Linux
# base (e.g. test a new image revision before promoting it).
ARG MARINER_IMAGE=mcr.microsoft.com/azurelinux/base/core:3.0

# Create a builder image with the compilers, etc. needed
FROM ${MARINER_IMAGE} AS build-env

# Install all the required packages for building. This list is probably
# longer than necessary.
RUN echo "== Install Git/CA certificates ==" && \
    tdnf install -y \
        git \
        ca-certificates

RUN echo "== Install Core dependencies ==" && \
    tdnf install -y \
        alsa-lib \
        alsa-lib-devel  \
        autoconf  \
        automake  \
        binutils  \
        bison  \
        build-essential  \
        cairo \
        cairo-devel \
        clang  \
        clang-devel  \
        cmake  \
        dbus  \
        dbus-devel  \
        dbus-glib  \
        dbus-glib-devel  \
        diffutils  \
        elfutils-devel  \
        file-libs  \
        flex  \
        fontconfig-devel  \
        gawk  \
        gcc  \
        gettext  \
        glibc-devel  \
        glib-schemas \
        gobject-introspection  \
        gobject-introspection-devel  \
        harfbuzz  \
        harfbuzz-devel  \
        kernel-headers  \
        intltool \
        libatomic_ops  \
        libcap-devel  \
        libffi  \
        libffi-devel  \
        libgudev  \
        libgudev-devel  \
        libjpeg-turbo  \
        libjpeg-turbo-devel  \
        libltdl  \
        libltdl-devel  \
        libpng-devel  \
        librsvg2-devel \
        libtiff  \
        libtiff-devel  \
        libusb  \
        libusb-devel  \
        libwebp  \
        libwebp-devel  \
        libxml2 \
        libxml2-devel  \
        make  \
        meson  \
        newt  \
        nss  \
        nss-libs  \
        openldap  \
        openssl-devel  \
        pam-devel  \
        pango  \
        pango-devel  \
        patch  \
        perl-XML-Parser \
        polkit-devel  \
        python3-devel \
        python3-mako  \
        python3-markupsafe \
        sed \
        sqlite-devel \
        systemd-devel  \
        tar \
        unzip  \
        vala  \
        vala-devel  \
        vala-tools  \
        zlib-devel

RUN echo "== Install UI dependencies ==" && \
    tdnf    install -y \
            libdrm-devel \
            libepoxy-devel \
            libevdev \
            libevdev-devel \
            libinput \
            libinput-devel \
            libpciaccess-devel \
            libSM-devel \
            libsndfile \
            libsndfile-devel \
            libXcursor \
            libXcursor-devel \
            libXdamage-devel \
            libXfont2-devel \
            libXi \
            libXi-devel \
            libxkbcommon-devel \
            libxkbfile-devel \
            libXrandr-devel \
            libxshmfence-devel \
            libXtst \
            libXtst-devel \
            libXxf86vm-devel \
            wayland-devel \
            wayland-protocols-devel \
            xkbcomp \
            xkeyboard-config \
            xorg-x11-server-Xwayland-devel \
            xorg-x11-util-macros

# Create an image with builds of FreeRDP and Weston
FROM build-env AS dev

ARG WSLG_VERSION="<current>"
ARG WSLG_COMMIT="<unknown>"
ARG WSLG_ARCH="x86_64"
ARG DIRECTX_HEADERS_VERSION="<unknown>"
ARG FREERDP_COMMIT="<unknown>"
ARG MESA_VERSION="<unknown>"
ARG PULSEAUDIO_COMMIT="<unknown>"
ARG WESTON_COMMIT="<unknown>"
ARG SYSTEMDISTRO_DEBUG_BUILD
ARG FREERDP_VERSION=2

# Fail fast if any required --build-arg is missing or still holds a
# placeholder value. We have to validate up-front because the values
# flow straight into /etc/versions.txt for supportability; a build
# that silently produced "wslg: <unknown>" in the VHD was previously
# a real footgun (CI misconfig surfacing 30 minutes into the build
# instead of in the first step).
#
# Accepts either the angle-bracketed defaults declared above or the
# bare 'unknown' that build-and-export.sh falls back to when a vendor
# dir was sourced from a tarball without git metadata. If you really
# need a partial build for local prototyping, pass --build-arg
# WSLG_VERSION=dev (etc.) explicitly so the rejection here is opt-out
# rather than accidental.
RUN set -e; \
    for kv in "WSLG_VERSION=${WSLG_VERSION}" \
              "WSLG_COMMIT=${WSLG_COMMIT}" \
              "WSLG_ARCH=${WSLG_ARCH}" \
              "DIRECTX_HEADERS_VERSION=${DIRECTX_HEADERS_VERSION}" \
              "FREERDP_COMMIT=${FREERDP_COMMIT}" \
              "MESA_VERSION=${MESA_VERSION}" \
              "PULSEAUDIO_COMMIT=${PULSEAUDIO_COMMIT}" \
              "WESTON_COMMIT=${WESTON_COMMIT}"; do \
        name=${kv%%=*}; val=${kv#*=}; \
        case "$val" in \
            ""|"<unknown>"|"<current>"|"unknown") \
                echo "ERROR: required --build-arg $name is unset or a placeholder ('$val')." >&2; \
                echo "       Pass an explicit value, or run ./build-and-export.sh from the wslg/" >&2; \
                echo "       checkout (see CONTRIBUTING.md for the manual docker build recipe)." >&2; \
                exit 1 ;; \
        esac; \
    done; \
    echo "All 8 required --build-arg values present."

WORKDIR /work
RUN printf 'WSLg: %s\nArchitecture: %s\nBuilt: %s\nOS: %s\n\n' \
        "${WSLG_VERSION}" \
        "${WSLG_ARCH}" \
        "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
        "$(. /etc/os-release && echo "${PRETTY_NAME}")" \
        > /work/versions.txt && \
    printf '%-16s %s\n' \
        'wslg:'            "${WSLG_COMMIT}" \
        'DirectX-Headers:' "${DIRECTX_HEADERS_VERSION}" \
        'FreeRDP:'         "${FREERDP_COMMIT}" \
        'mesa:'            "${MESA_VERSION}" \
        'pulseaudio:'      "${PULSEAUDIO_COMMIT}" \
        'weston:'          "${WESTON_COMMIT}" \
        >> /work/versions.txt

#
# Build runtime dependencies.
#

ENV BUILDTYPE=${SYSTEMDISTRO_DEBUG_BUILD:+debug}
ENV BUILDTYPE=${BUILDTYPE:-debugoptimized}

ENV BUILDTYPE_NODEBUGSTRIP=${SYSTEMDISTRO_DEBUG_BUILD:+debug}
ENV BUILDTYPE_NODEBUGSTRIP=${BUILDTYPE_NODEBUGSTRIP:-release}

# FreeRDP is always built with RelWithDebInfo
ENV BUILDTYPE_FREERDP=${BUILDTYPE_FREERDP:-RelWithDebInfo}

ENV WITH_DEBUG_FREERDP=${SYSTEMDISTRO_DEBUG_BUILD:+ON}
ENV WITH_DEBUG_FREERDP=${WITH_DEBUG_FREERDP:-OFF}

RUN echo "== System distro build types ==" && \
    echo "    BUILDTYPE:              ${BUILDTYPE}" && \
    echo "    BUILDTYPE_NODEBUGSTRIP: ${BUILDTYPE_NODEBUGSTRIP}" && \
    echo "    BUILDTYPE_FREERDP:      ${BUILDTYPE_FREERDP}" && \
    echo "    WITH_DEBUG_FREERDP:     ${WITH_DEBUG_FREERDP}"

ENV DESTDIR=/work/build
ENV PREFIX=/usr
ENV PKG_CONFIG_PATH=${DESTDIR}${PREFIX}/lib/pkgconfig:${DESTDIR}${PREFIX}/lib/${WSLG_ARCH}-linux-gnu/pkgconfig:${DESTDIR}${PREFIX}/share/pkgconfig
ENV C_INCLUDE_PATH=${DESTDIR}${PREFIX}/include/freerdp${FREERDP_VERSION}:${DESTDIR}${PREFIX}/include/winpr${FREERDP_VERSION}:${DESTDIR}${PREFIX}/include/wsl/stubs:${DESTDIR}${PREFIX}/include
ENV CPLUS_INCLUDE_PATH=${C_INCLUDE_PATH}
ENV LIBRARY_PATH=${DESTDIR}${PREFIX}/lib
ENV LD_LIBRARY_PATH=${LIBRARY_PATH}
ENV CC=/usr/bin/gcc
ENV CXX=/usr/bin/g++

# Setup DebugInfo folder
COPY debuginfo /work/debuginfo
RUN chmod +x /work/debuginfo/*.sh

# Build DirectX-Headers
COPY vendor/DirectX-Headers-1.0 /work/vendor/DirectX-Headers-1.0
WORKDIR /work/vendor/DirectX-Headers-1.0
RUN /usr/bin/meson --prefix=${PREFIX} build \
        --buildtype=${BUILDTYPE_NODEBUGSTRIP} \
        -Dbuild-test=false && \
    ninja -C build -j8 install

# Build mesa with the minimal options we need.
COPY vendor/mesa /work/vendor/mesa
WORKDIR /work/vendor/mesa
RUN /usr/bin/meson --prefix=${PREFIX} build \
        --buildtype=${BUILDTYPE_NODEBUGSTRIP} \
        -Dgallium-drivers=swrast,d3d12 \
        -Dvulkan-drivers= \
        -Dllvm=disabled && \
    ninja -C build -j8 install

# Build PulseAudio
COPY vendor/pulseaudio /work/vendor/pulseaudio
WORKDIR /work/vendor/pulseaudio
RUN /usr/bin/meson --prefix=${PREFIX} build \
        --buildtype=${BUILDTYPE_NODEBUGSTRIP} \
        -Ddatabase=simple \
        -Ddoxygen=false \
        -Dgsettings=disabled \
        -Dtests=false && \
    ninja -C build -j8 install

# Build FreeRDP
COPY vendor/FreeRDP /work/vendor/FreeRDP
WORKDIR /work/vendor/FreeRDP
RUN cmake -G Ninja \
        -B build \
        -DCMAKE_INSTALL_PREFIX=${PREFIX} \
        -DCMAKE_INSTALL_LIBDIR=${PREFIX}/lib \
        -DCMAKE_BUILD_TYPE=${BUILDTYPE_FREERDP} \
        -DWITH_DEBUG_ALL=${WITH_DEBUG_FREERDP} \
        -DWITH_ICU=ON \
        -DWITH_SERVER=ON \
        -DWITH_CHANNEL_GFXREDIR=ON \
        -DWITH_CHANNEL_RDPAPPLIST=ON \
        -DWITH_CLIENT=OFF \
        -DWITH_CLIENT_COMMON=OFF \
        -DWITH_CLIENT_CHANNELS=OFF \
        -DWITH_CLIENT_INTERFACE=OFF \
        -DWITH_LIBSYSTEMD=OFF \
        -DWITH_WAYLAND=OFF \
        -DWITH_X11=OFF \
        -DWITH_PROXY=OFF \
        -DWITH_SHADOW=OFF \
        -DWITH_SAMPLE=OFF && \
    ninja -C build -j8 install

RUN /work/debuginfo/strip_debuginfo.sh "FreeRDP" "/work/debuginfo/FreeRDP${FREERDP_VERSION}.list"

# Build rdpapplist RDP virtual channel plugin
COPY rdpapplist /work/rdpapplist
WORKDIR /work/rdpapplist
RUN /usr/bin/meson --prefix=${PREFIX} build \
        --buildtype=${BUILDTYPE} && \
    ninja -C build -j8 install

RUN /work/debuginfo/strip_debuginfo.sh "rdpapplist" "/work/debuginfo/rdpapplist.list"

# Build Weston
COPY vendor/weston /work/vendor/weston
WORKDIR /work/vendor/weston
RUN /usr/bin/meson --prefix=${PREFIX} build \
        --buildtype=${BUILDTYPE} \
        -Dbackend-default=rdp \
        -Dbackend-drm=false \
        -Dbackend-drm-screencast-vaapi=false \
        -Dbackend-headless=false \
        -Dbackend-wayland=false \
        -Dbackend-x11=false \
        -Dbackend-fbdev=false \
        -Dcolor-management-colord=false \
        -Dscreenshare=false \
        -Dsystemd=false \
        -Dwslgd=true \
        -Dremoting=false \
        -Dpipewire=false \
        -Dshell-fullscreen=false \
        -Dcolor-management-lcms=false \
        -Dshell-ivi=false \
        -Dshell-kiosk=false \
        -Ddemo-clients=false \
        -Dsimple-clients=[] \
        -Dtools=[] \
        -Dresize-pool=false \
        -Dwcap-decode=false \
        -Dtest-junit-xml=false && \
    ninja -C build -j8 install

RUN /work/debuginfo/strip_debuginfo.sh "weston" "/work/debuginfo/weston.list"

# Build WSLGd Daemon
ENV CC=/usr/bin/clang
ENV CXX=/usr/bin/clang++

COPY WSLGd /work/WSLGd
WORKDIR /work/WSLGd
RUN /usr/bin/meson --prefix=${PREFIX} build \
        --buildtype=${BUILDTYPE} && \
    ninja -C build -j8 install

RUN /work/debuginfo/strip_debuginfo.sh "WSLGd" "/work/debuginfo/WSLGd.list"

# Gather debuginfo to a tar file. strip_debuginfo.sh above already
# populated /work/build/debuginfo with per-binary .debug files; this
# is a no-op for SYSTEMDISTRO_DEBUG_BUILD builds because nothing was
# split out in the first place.
RUN if [ -z "$SYSTEMDISTRO_DEBUG_BUILD" ] ; then \
        echo "== Compress debug info: /work/debuginfo/system-debuginfo.tar.gz ==" && \
        tar -C /work/build/debuginfo -czf /work/debuginfo/system-debuginfo.tar.gz ./ ; \
    fi

########################################################################
########################################################################

## Create the distro image with just what's needed at runtime

FROM ${MARINER_IMAGE} AS runtime

RUN echo "== Install Core/UI Runtime Dependencies ==" && \
    tdnf    install -y \
            busybox \
            ca-certificates \
            cairo \
            chrony \
            containerd2 \
            containernetworking-plugins \
            runc \
            dbus \
            dbus-glib \
            dhcpcd \
            docker-buildx \
            docker-cli \
            e2fsprogs \
            freefont \
            gzip \
            icu \
            iptables \
            kmod \
            libinput \
            libjpeg-turbo \
            libltdl \
            libpng \
            librsvg2 \
            libsndfile \
            libwayland-client \
            libwayland-server \
            libwayland-cursor \
            libwebp \
            libXcursor \
            libxkbcommon \
            libXrandr \
            iproute \
            moby-engine \
            nftables \
            pango \
            procps-ng \
            rpm \
            sed \
            systemd-libs \
            tar \
            tzdata \
            util-linux \
            xcursor-themes \
            xorg-x11-server-Xwayland \
            xorg-x11-server-utils

# Install busybox utilities
RUN /sbin/busybox --install -s

# Remove unnecessary packages and files to reduce image size
ARG SYSTEMDISTRO_DEBUG_BUILD
RUN if [ -z "$SYSTEMDISTRO_DEBUG_BUILD" ] ; then \
        echo "== Removing unnecessary packages ==" && \
        # Remove build tools and packages not needed at runtime \
        rpm -e --nodeps \
            cracklib-dicts \
            gcc \
            gcc-c++ \
            libpkgconf \
            llvm \
            perl \
            pkgconf \
            pkgconf-m4 \
            pkgconf-pkg-config \
            python3 \
            python3-libs && \
        # Remove all perl subpackages \
        rpm -e --nodeps $(rpm -qa | grep -- '^perl-') && \
        # Remove all -devel packages \
        rpm -e --nodeps $(rpm -qa | grep -- '-devel') && \
        # Remove systemd components (except systemd-libs which is needed by weston) \
        rpm -e --nodeps $(rpm -qa | grep -- '^systemd-' | grep -v systemd-libs) && \
        # Remove orphaned packages \
        tdnf autoremove -y && \
        echo "== Removing unnecessary files ==" && \
        # Remove docs, man pages, locales, gtk-doc \
        rm -rf /usr/share/man /usr/share/info /usr/share/locale /usr/share/gtk-doc && \
        find /usr/share/doc -mindepth 1 -maxdepth 1 -type d -exec rm -rf {} + && \
        # Remove unused Mesa driver \
        rm -f /usr/lib64/dri/virtio_gpu_dri.so && \
        # Remove hardware database (not needed in WSL) \
        rm -rf /usr/share/hwdata/* && \
        # Remove temporary files, logs, caches, and systemd catalog \
        rm -rf /tmp/* /var/tmp/* /var/log/* /var/cache/* /usr/lib/systemd/catalog/*; \
    else \
        echo "== Install development aid packages ==" && \
        tdnf install -y \
             gdb \
             azurelinux-repos-debug \
             nano \
             vim \
             wayland-debuginfo \
             xorg-x11-server-debuginfo; \
    fi

# Clear the tdnf cache to make the image smaller
RUN tdnf clean all

# Create wslg user.
RUN useradd -u 1000 --create-home wslg && \
    mkdir /home/wslg/.config && \
    chown wslg /home/wslg/.config

# Copy config files.
COPY config/wsl.conf /etc/wsl.conf
COPY config/weston.ini /home/wslg/.config/weston.ini
COPY config/local.conf /etc/fonts/local.conf

# Copy default icon file.
COPY resources/linux.png /usr/share/icons/wsl/linux.png

# Copy the built artifacts from the build stage.
COPY --from=dev /work/build/usr/ /usr/
COPY --from=dev /work/build/etc/ /etc/

# Append WSLg setttings to pulseaudio.
COPY config/default_wslg.pa /etc/pulse/default_wslg.pa
RUN cat /etc/pulse/default_wslg.pa >> /etc/pulse/default.pa
RUN rm /etc/pulse/default_wslg.pa

# Copy the licensing information for PulseAudio
COPY --from=dev /work/vendor/pulseaudio/GPL \
                /work/vendor/pulseaudio/LGPL \
                /work/vendor/pulseaudio/LICENSE \
                /work/vendor/pulseaudio/NEWS \
                /work/vendor/pulseaudio/README /usr/share/doc/pulseaudio/

# Copy the licensing information for Weston
COPY --from=dev /work/vendor/weston/COPYING /usr/share/doc/weston/COPYING

# Copy the licensing information for FreeRDP
COPY --from=dev /work/vendor/FreeRDP/LICENSE /usr/share/doc/FreeRDP/LICENSE

# copy the documentation and licensing information for mesa
COPY --from=dev /work/vendor/mesa/docs /usr/share/doc/mesa/

COPY --from=dev /work/versions.txt /etc/versions.txt

CMD /usr/bin/WSLGd
