# WSLg — Copilot instructions

WSLg is the **system distro** that backs GUI/audio in WSL 2: a containerized Azure Linux 3.0 image running Weston (RDP backend), XWayland, PulseAudio, and the `WSLGd` supervisor, plus a Windows-side `mstsc` plugin (`WSLDVCPlugin`). This repo packages, builds, and assembles those pieces — it is **not** a typical application repo.

## What lives here

- `Dockerfile` — multi-stage build (`build-env` → `dev` → `runtime`) that produces the system distro image.
- `build-and-export.sh` — preferred local entry point. Derives versions, invokes `docker build`, exports the container, and converts to `system_x64.vhd` via `tar2ext4`.
- `WSLGd/` — C++17 supervisor daemon (PID 2 in the system distro). Built with Meson; falls back to a `Makefile` for standalone builds.
- `rdpapplist/` — C FreeRDP virtual-channel server plugin (Meson). Weston loads it to push the Linux app list to the Windows start menu.
- `WSLDVCPlugin/` — Windows-side mstsc dynamic virtual-channel plugin (Visual Studio solution, `WSLDVCPlugin.sln`). Built separately on Windows, not in the Docker build.
- `vendor/` — **not checked in.** Contributors must clone the mirrored sources here before building (see CONTRIBUTING.md).
- `config/`, `resources/`, `package/` — files copied into the runtime image / NuGet package.
- `debuginfo/` — `.list` files enumerating binaries to split, plus `strip_debuginfo.sh` / `gen_debuginfo.sh` invoked from the Dockerfile.
- `devops/` — version-string helpers used by both `build-and-export.sh` and CI.
- `Microsoft.WSLg.nuspec` / `Microsoft.WSLg.targets` — the NuGet package that ships the x64 + arm64 VHDs and `WSLDVCPlugin.dll` to Windows.

## Build / test / lint

There is **no application-style test suite or linter**. "Building" means producing a VHD. Always build inside Linux/WSL 2 with Docker running.

**Vendor checkout (required before any build).** Clone into `vendor/` using the **`working`** branch of each mirror — not `main` (`main` tracks upstream and won't build):

```bash
git clone https://github.com/microsoft/FreeRDP-mirror   vendor/FreeRDP                -b working
git clone https://github.com/microsoft/weston-mirror    vendor/weston                 -b working
git clone https://github.com/microsoft/PulseAudio-mirror vendor/pulseaudio            -b working
git clone https://github.com/microsoft/DirectX-Headers   vendor/DirectX-Headers-1.0   -b v1.608.0
git clone https://gitlab.freedesktop.org/mesa/mesa       vendor/mesa                  -b mesa-23.1.0
```

**Build the VHD (preferred):**

```bash
./build-and-export.sh           # → system_x64.vhd
```

**Manual `docker build`:** all **7** vendor/version `--build-arg`s are required (`WSLG_VERSION`, `WSLG_COMMIT`, `DIRECTX_HEADERS_VERSION`, `FREERDP_COMMIT`, `MESA_VERSION`, `PULSEAUDIO_COMMIT`, `WESTON_COMMIT`). The first `RUN` in the `dev` stage **fails fast** if any is empty or holds a placeholder (`""`, `<unknown>`, `<current>`, `unknown`). The literal `"dev"` is the accepted local-fallback sentinel (used by `build-and-export.sh` for tarball checkouts); do not substitute `unknown`. `WSLG_ARCH` defaults to `x86_64`.

**Debug build:** add `--build-arg SYSTEMDISTRO_DEBUG_BUILD=true`. This sets Meson `buildtype=debug`, FreeRDP `WITH_DEBUG_ALL=ON`, keeps symbols inline (skips `strip_debuginfo.sh`), and installs `gdb`/`nano`/`vim`/debuginfo packages into the runtime image.

**Individual components** (only useful when iterating on one piece — the production build always goes through the Dockerfile):

```bash
cd WSLGd     && meson build && ninja -C build        # or `make` via the fallback Makefile
cd rdpapplist && meson build && ninja -C build
```

`WSLDVCPlugin` builds on Windows via Visual Studio (open `WSLDVCPlugin.sln`).

**Try out a local VHD:** point `%USERPROFILE%\.wslconfig` at it and restart WSL — see CONTRIBUTING.md.

```
[wsl2]
systemDistro=C:\path\to\system_x64.vhd
```

## Architecture (the big picture)

The Dockerfile is the source of truth for the runtime layout. Components are built **in this order** inside the `dev` stage and the install tree at `/work/build` is copied wholesale into the `runtime` stage:

1. **DirectX-Headers** → 2. **Mesa** → 3. **PulseAudio** → 4. **FreeRDP** → 5. **rdpapplist** → 6. **Weston** → 7. **WSLGd**

Order matters: Mesa and DirectX-Headers feed Weston's GL path; FreeRDP + rdpapplist must be installed before Weston links against them; WSLGd links `winpr` from FreeRDP.

Most components are built with deliberately stripped option sets (don't "fix" these — they are intentional minimisations for VHD size / attack surface):

- **Mesa:** only `gallium-drivers=swrast,d3d12`; `vulkan-drivers=` empty; `llvm=disabled`. (`virtio_gpu_dri.so` is also deleted from the runtime image.)
- **Weston:** `backend-default=rdp`; every other backend (drm, headless, wayland, x11, fbdev) and every shell except `rdprail-shell` is disabled; `systemd=false`, `wslgd=true`, `pipewire=false`, `remoting=false`, demo/simple/tools all off.
- **FreeRDP:** **server-only** build (`WITH_SERVER=ON`, all `WITH_CLIENT*=OFF`, no X11/Wayland/proxy/shadow); built with `RelWithDebInfo` regardless of debug flag; `WITH_CHANNEL_GFXREDIR` and `WITH_CHANNEL_RDPAPPLIST` are required-on.
- **PulseAudio:** `database=simple`, `tests=false`, `doxygen=false`, `gsettings=disabled`.

**Runtime flow.** `wsl.conf` sets `command=/usr/bin/WSLGd` as the boot command running as user `wslg`. `WSLGd` then launches Weston (`rdprail-shell` by default, `desktop-shell` if requested), PulseAudio, and an RDP client on the Windows side (`msrdc.exe` or `mstsc.exe`), and monitors them — it restarts crashed children. Shared state lives under `/mnt/wslg` (the `SHARE_PATH` macro in `WSLGd/common.h`); the user distro is mounted under `/mnt/wslg/distro`. Both `wslg.rdp` and `wslg_desktop.rdp` are committed under `package/`.

**Vendor mirrors are the contribution path** for Weston / FreeRDP / PulseAudio changes — branch `working` carries in-flight patches that haven't landed upstream yet. Don't edit those projects in this repo; they're sourced from `vendor/`.

## Conventions worth knowing

- **WSLGd logging:** use `LOG_ERROR(fmt, ...)` / `LOG_INFO(fmt, ...)` from `WSLGd/common.h`, not raw `fprintf`. They prefix timestamp + function + line.
- **FreeRDP/WinPR version probing:** both `WSLGd/meson.build` and `rdpapplist/meson.build` try `freerdp3`/`winpr3` first, then fall back to v2. Keep new dependencies on FreeRDP behind the same fallback.
- **Versions baked into the image:** `/etc/versions.txt` is generated in the `dev` stage from the seven `--build-arg`s. The fail-fast loop exists because a silent `<unknown>` ended up in shipped VHDs in the past — keep that behaviour.
- **Debuginfo split:** every built component runs `debuginfo/strip_debuginfo.sh <label> <list-file>` after install, which respects `SYSTEMDISTRO_DEBUG_BUILD` and writes per-binary `.debug` files that are tarred into `system-debuginfo.tar.gz`. If you add a new built component, add a `.list` file under `debuginfo/` and a matching `strip_debuginfo.sh` call.
- **Runtime image is aggressively pruned** in the non-debug path (RPM removes for `gcc`, `python3`, all `perl-*`, all `*-devel`, most `systemd-*`, plus `man`/`info`/`locale`/`gtk-doc` trees). Anything you need at runtime must survive that purge or be reinstalled in the debug-only branch.
- **Read-only at runtime.** The system distro is mounted read-only; runtime changes are discarded on `wsl --shutdown`. To add a tool permanently, edit the `Dockerfile`, not the running instance.
- **Paths are POSIX inside the VHD** even though the rest of this repo (nuspec, WSLDVCPlugin, .targets) is Windows-shaped. Don't conflate the two halves.
