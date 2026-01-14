# Create a builder image with the compilers, etc. needed
FROM mcr.microsoft.com/azurelinux/base/core:3.0 AS build-env

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
ARG WSLG_ARCH="x86_64"
ARG SYSTEMDISTRO_DEBUG_BUILD
ARG FREERDP_VERSION=2

WORKDIR /work
RUN echo "WSLg (" ${WSLG_ARCH} "):" ${WSLG_VERSION} > /work/versions.txt
RUN echo "Built at:" `date --utc` >> /work/versions.txt

RUN echo "Azure Linux:" `cat /etc/os-release | head -2 | tail -1` >> /work/versions.txt

#
# Build runtime dependencies.
#

ENV BUILDTYPE=${SYSTEMDISTRO_DEBUG_BUILD:+debug}
ENV BUILDTYPE=${BUILDTYPE:-debugoptimized}
RUN echo "== System distro build type:" ${BUILDTYPE} " =="

ENV BUILDTYPE_NODEBUGSTRIP=${SYSTEMDISTRO_DEBUG_BUILD:+debug}
ENV BUILDTYPE_NODEBUGSTRIP=${BUILDTYPE_NODEBUGSTRIP:-release}
RUN echo "== System distro build type (no debug strip):" ${BUILDTYPE_NODEBUGSTRIP} " =="

# FreeRDP is always built with RelWithDebInfo
ENV BUILDTYPE_FREERDP=${BUILDTYPE_FREERDP:-RelWithDebInfo}
RUN echo "== System distro build type (FreeRDP):" ${BUILDTYPE_FREERDP} " =="

ENV WITH_DEBUG_FREERDP=${SYSTEMDISTRO_DEBUG_BUILD:+ON}
ENV WITH_DEBUG_FREERDP=${WITH_DEBUG_FREERDP:-OFF}
RUN echo "== System distro build type (FreeRDP Debug Options):" ${WITH_DEBUG_FREERDP} " =="

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
RUN chmod +x /work/debuginfo/gen_debuginfo.sh

# Build DirectX-Headers
COPY vendor/DirectX-Headers-1.0 /work/vendor/DirectX-Headers-1.0
WORKDIR /work/vendor/DirectX-Headers-1.0
RUN /usr/bin/meson --prefix=${PREFIX} build \
        --buildtype=${BUILDTYPE_NODEBUGSTRIP} \
        -Dbuild-test=false && \
    ninja -C build -j8 install && \
    echo 'DirectX-Headers:' `git --git-dir=/work/vendor/DirectX-Headers-1.0/.git rev-parse --verify HEAD` >> /work/versions.txt

# Build mesa with the minimal options we need.
COPY vendor/mesa /work/vendor/mesa
WORKDIR /work/vendor/mesa
RUN /usr/bin/meson --prefix=${PREFIX} build \
        --buildtype=${BUILDTYPE_NODEBUGSTRIP} \
        -Dgallium-drivers=swrast,d3d12 \
        -Dvulkan-drivers= \
        -Dllvm=disabled && \
    ninja -C build -j8 install && \
    echo 'mesa:' `git --git-dir=/work/vendor/mesa/.git rev-parse --verify HEAD` >> /work/versions.txt

# Build PulseAudio
COPY vendor/pulseaudio /work/vendor/pulseaudio
WORKDIR /work/vendor/pulseaudio
RUN /usr/bin/meson --prefix=${PREFIX} build \
        --buildtype=${BUILDTYPE_NODEBUGSTRIP} \
        -Ddatabase=simple \
        -Ddoxygen=false \
        -Dgsettings=disabled \
        -Dtests=false && \
    ninja -C build -j8 install && \
    echo 'pulseaudio:' `git --git-dir=/work/vendor/pulseaudio/.git rev-parse --verify HEAD` >> /work/versions.txt

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
    ninja -C build -j8 install && \
    echo 'FreeRDP:' `git --git-dir=/work/vendor/FreeRDP/.git rev-parse --verify HEAD` >> /work/versions.txt

WORKDIR /work/debuginfo
RUN if [ -z "$SYSTEMDISTRO_DEBUG_BUILD" ] ; then \
        echo "== Strip debug info: FreeRDP ==" && \
        /work/debuginfo/gen_debuginfo.sh /work/debuginfo/FreeRDP${FREERDP_VERSION}.list /work/build; \
    fi

# Build rdpapplist RDP virtual channel plugin
COPY rdpapplist /work/rdpapplist
WORKDIR /work/rdpapplist
RUN /usr/bin/meson --prefix=${PREFIX} build \
        --buildtype=${BUILDTYPE} && \
    ninja -C build -j8 install

WORKDIR /work/debuginfo
RUN if [ -z "$SYSTEMDISTRO_DEBUG_BUILD" ] ; then \
        echo "== Strip debug info: rdpapplist ==" && \
        /work/debuginfo/gen_debuginfo.sh /work/debuginfo/rdpapplist.list /work/build; \
    fi

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
    ninja -C build -j8 install && \
    echo 'weston:' `git --git-dir=/work/vendor/weston/.git rev-parse --verify HEAD` >> /work/versions.txt

WORKDIR /work/debuginfo
RUN if [ -z "$SYSTEMDISTRO_DEBUG_BUILD" ] ; then \
        echo "== Strip debug info: weston ==" && \
        /work/debuginfo/gen_debuginfo.sh /work/debuginfo/weston.list /work/build; \
    fi

# Build WSLGd Daemon
ENV CC=/usr/bin/clang
ENV CXX=/usr/bin/clang++

COPY WSLGd /work/WSLGd
WORKDIR /work/WSLGd
RUN /usr/bin/meson --prefix=${PREFIX} build \
        --buildtype=${BUILDTYPE} && \
    ninja -C build -j8 install

WORKDIR /work/debuginfo
RUN if [ -z "$SYSTEMDISTRO_DEBUG_BUILD" ] ; then \
        echo "== Strip debug info: WSLGd ==" && \
        /work/debuginfo/gen_debuginfo.sh /work/debuginfo/WSLGd.list /work/build; \
    fi

# Gather debuginfo to a tar file
WORKDIR /work/debuginfo
RUN if [ -z "$SYSTEMDISTRO_DEBUG_BUILD" ] ; then \
        echo "== Compress debug info: /work/debuginfo/system-debuginfo.tar.gz ==" && \
        tar -C /work/build/debuginfo -czf system-debuginfo.tar.gz ./ ; \
    fi

########################################################################
########################################################################

## Create the distro image with just what's needed at runtime

FROM mcr.microsoft.com/azurelinux/base/core:3.0 AS runtime

RUN echo "== Install Core/UI Runtime Dependencies ==" && \
    tdnf    install -y \
            busybox \
            cairo \
            chrony \
            dbus \
            dbus-glib \
            dhcpcd \
            e2fsprogs \
            freefont \
            icu \
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
            nftables \
            pango \
            procps-ng \
            rpm \
            sed \
            systemd-libs \
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
        rm -rf /usr/share/hwdata/*; \
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
