# Create a builder image with the compilers, etc. needed
FROM cblmariner.azurecr.io/base/core:1.0.20210224 AS build-env

# Install all the required packages for building. This list is probably
# longer than necessary.
RUN echo " Install Git/CA certificates "
RUN tdnf install -y \
        git \
        ca-certificates

RUN echo " Install mariner-repos-ui REPO"
RUN tdnf install -y mariner-repos-ui

RUN echo " Install Core dependencies "
RUN tdnf install -y \
        alsa-lib \
        alsa-lib-devel  \
        autoconf  \
        automake  \
        binutils  \
        bison  \
        build-essential  \
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
        python2-devel \
        python3-mako  \
        sqlite-devel \
        systemd-devel  \
        UI-cairo \
        UI-cairo-devel \
        unzip  \
        vala  \
        vala-devel  \
        vala-tools

RUN echo " Install UI dependencies "
RUN tdnf    install -y \
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
            xorg-x11-server-devel \
            xorg-x11-util-macros


# Create an image with builds of FreeRDP and Weston
FROM build-env AS dev

ARG WSLG_VERSION="<current>"
ARG WSLG_ARCH="x86_64"

WORKDIR /work
RUN echo "WSLg (" ${WSLG_ARCH} "):" ${WSLG_VERSION} > /work/versions.txt

RUN echo "Mariner:" `cat /etc/os-release | head -2 | tail -1` >> /work/versions.txt

#
# Build runtime dependencies.
#

ENV BUILDTYPE=release
ENV DESTDIR=/work/build
ENV PREFIX=/usr
ENV PKG_CONFIG_PATH=${DESTDIR}${PREFIX}/lib/pkgconfig:${DESTDIR}${PREFIX}/lib/${WSLG_ARCH}-linux-gnu/pkgconfig:${DESTDIR}${PREFIX}/share/pkgconfig
ENV C_INCLUDE_PATH=${DESTDIR}${PREFIX}/include/freerdp2:${DESTDIR}${PREFIX}/include/winpr2
ENV CPLUS_INCLUDE_PATH=${C_INCLUDE_PATH}
ENV LIBRARY_PATH=${DESTDIR}${PREFIX}/lib
ENV LD_LIBRARY_PATH=${LIBRARY_PATH}
ENV CC=/usr/bin/gcc
ENV CXX=/usr/bin/g++

# Build FreeRDP
COPY vendor/FreeRDP /work/vendor/FreeRDP
WORKDIR /work/vendor/FreeRDP
RUN cmake -G Ninja \
        -B build \
        -DCMAKE_INSTALL_PREFIX=${PREFIX} \
        -DCMAKE_INSTALL_LIBDIR=${PREFIX}/lib \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DWITH_SERVER=ON \
        -DWITH_CHANNEL_GFXREDIR=ON \
        -DWITH_CHANNEL_RDPAPPLIST=ON \
        -DWITH_CLIENT=OFF \
        -DWITH_CLIENT_COMMON=OFF \
        -DWITH_CLIENT_CHANNELS=OFF \
        -DWITH_CLIENT_INTERFACE=OFF \
        -DWITH_PROXY=OFF \
        -DWITH_SHADOW=OFF \
        -DWITH_SAMPLE=OFF && \
    ninja -C build -j8 install
RUN echo 'FreeRDP:' `git --git-dir=/work/vendor/FreeRDP/.git rev-parse --verify HEAD` >> /work/versions.txt

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
RUN echo 'weston:' `git --git-dir=/work/vendor/weston/.git rev-parse --verify HEAD` >> /work/versions.txt

# Build PulseAudio
COPY vendor/pulseaudio /work/vendor/pulseaudio
WORKDIR /work/vendor/pulseaudio
RUN /usr/bin/meson --prefix=${PREFIX} build \
        --buildtype=${BUILDTYPE} \
        -Ddatabase=simple \
        -Dbluez5=false \
        -Dgsettings=disabled \
        -Dtests=false && \
    ninja -C build -j8 install
RUN echo 'pulseaudio:' `git --git-dir=/work/vendor/pulseaudio/.git rev-parse --verify HEAD` >> /work/versions.txt

# Build mesa with the minimal options we need.
COPY vendor/mesa /work/vendor/mesa
WORKDIR /work/vendor/mesa
RUN /usr/bin/meson --prefix=${PREFIX} build \
        --buildtype=${BUILDTYPE} \
        -Dgallium-drivers=swrast,d3d12 \
        -Ddri-drivers= \
        -Dvulkan-drivers= \
        -Dllvm=disabled && \
    ninja -C build -j8 install
RUN echo 'mesa:' `git --git-dir=/work/vendor/mesa/.git rev-parse --verify HEAD` >> /work/versions.txt

ENV CC=/usr/bin/clang
ENV CXX=/usr/bin/clang++

# Build WSLGd Daemon
COPY WSLGd /work/WSLGd
WORKDIR /work/WSLGd
RUN /usr/bin/meson --prefix=${PREFIX} build && \
    ninja -C build -j8 install

########################################################################
########################################################################

## Create the distro image with just what's needed at runtime

FROM cblmariner.azurecr.io/base/core:1.0.20210224 AS runtime

RUN echo " Install mariner-repos-ui REPO"
RUN tdnf install -y mariner-repos-ui

RUN echo " Install Core/UI Runtime Dependencies "
RUN tdnf    install -y \
            dbus \
            dbus-glib \
            freefont \
            libinput \
            libjpeg-turbo \
            libltdl \
            libpng \
            libsndfile \
            libwayland-client \
            libwayland-server \
            libwayland-cursor \
            libwebp \
            libXcursor \
            libxkbcommon \
            libXrandr \
            nano \
            pango \
            procps-ng \
            tzdata \
            UI-cairo \
            wayland-protocols-devel \
            xcursor-themes \
            xorg-x11-server-Xwayland \
            xorg-x11-xtrans-devel

# Install packages to aid in development.
ARG SYSTEMDISTRO_DEBUG_BUILD
RUN if [ "$SYSTEMDISTRO_DEBUG_BUILD" = "on" ] ; then \
        tdnf install -y \
             gdb \
             vim ; \
    else                                            \
        rpm -e --nodeps python2                     \
        rpm -e --nodeps curl                        \
        rpm -e --nodeps python-xml                  \
        rpm -e --nodeps pkg-config                  \
        rpm -e --nodeps vim                         \
        rpm -e --nodeps wget                        \
        rpm -e --nodeps python3                     \
        rpm -e --nodeps python2-libs;               \
    fi

# Create wslg user.
RUN useradd -u 1000 --create-home wslg && \
    mkdir /home/wslg/.config && \
    chown wslg /home/wslg/.config

# Copy config files.
COPY config/wsl.conf /etc/wsl.conf
COPY config/weston.ini /home/wslg/.config/weston.ini

# Copy default icon file.
COPY resources/linux.png /usr/share/icons/wsl/linux.png

# Copy the built artifacts from the build stage.
COPY --from=dev /work/build/usr/ /usr/
COPY --from=dev /work/build/etc/ /etc/

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
