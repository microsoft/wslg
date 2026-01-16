## Quick orientation for AI coding agents

This repo implements WSLg (Windows Subsystem for Linux GUI). The goal of these notes is to help an automated coding assistant become productive fast: what the major components are, where to look for behavior, how developers build/test locally, and project-specific conventions.

### Big-picture architecture (high-level)
- WSLGd (`WSLGd/`) — first userland process launched in the system distro. It configures env variables, mounts shared resources, launches `weston`, `PulseAudio` and manages the RDP connection. See `WSLGd/main.cpp` for env keys, mount points and startup flow.
- Weston and related components — built from mirrors under `vendor/` (FreeRDP, weston, pulseaudio). See `config/BUILD.md` for vendor checkout and image build steps. The custom RDP backend and RAIL/VAIL logic live in the weston mirror.
- WSLDVCPlugin (`WSLDVCPlugin/`) — Windows-side plugin that receives app lists via a virtual channel and creates Start Menu links. This is a Visual C++/MSBuild solution (sln/vcxproj) — Windows build targets live here.
- Packaging and system distro — `container/` and `config/` contain Dockerfile, `tar2ext4` usage and instructions to produce `system.vhd` used as the system distro.

### Key integration points & runtime data flows
- RDP transport: Weston -> FreeRDP -> mstsc (Windows) (RAIL & VAIL modes). Look in `vendor/*-mirror` for the modified backends.
- Shared memory/virtio-fs: WSLG mounts a shared memory virtiofs at `/mnt/shared_memory` and uses it for VAIL transfers. Environment variables: `WSL2_SHARED_MEMORY_MOUNT_POINT` and `WSL2_SHARED_MEMORY_OB_DIRECTORY` (see `WSLGd/main.cpp`).
- Virtual channel: WSLDVCPlugin listens to the custom RDP virtual channel to enumerate GUI apps and create Windows shortcuts.

### Concrete developer workflows (discovered from repo files)
- Build Windows plugin: open `WSLDVCPlugin/WSLDVCPlugin.sln` in Visual Studio or run MSBuild on the `WSLDVCPlugin.vcxproj`. The repo workspace defines a build task for msbuild. On Windows (PowerShell), use `msbuild /t:build` or the provided VS solution.
- Build system distro (Linux/WSL): follow `config/BUILD.md` — clone FreeRDP/Weston/PulseAudio mirrors on `working` branch into `vendor/`, then build inside Docker (see `container/build.sh` and `config/BUILD.md`). After docker export, use `tar2ext4` (from hcsshim) to create `system.vhd`.
- Inspect runtime: run `wsl --system <DistroName>` and use `ps -ax | grep weston` or inspect logs under the shared mount: `/mnt/wslg/weston.log` and `/mnt/wslg/stderr.log` (see `WSLGd/main.cpp` for log path env overrides).

### Important files to reference when making changes
- `WSLGd/main.cpp` — daemon setup, env keys, mounts, logging, shared memory code paths.
- `WSLDVCPlugin/` — Windows side plugin, msbuild solution, resource files used to create Start Menu shortcuts.
- `config/BUILD.md` and `container/` — reproduce and debug the system distro build (Docker, tar->vhd steps).
- `package/` — contains `wslg.rdp` and `wslg_desktop.rdp` templates used by the project.
- `vendor/` mirrors — look here for modified upstream components (Weston, FreeRDP, PulseAudio).

### Project-specific conventions and patterns
- Mirror model: upstream projects are mirrored under `vendor/*-mirror`. The repo builds from the `working` branch of those mirrors. Search for `working` branch references when changing how an upstream component is built.
- Env-driven runtime behavior: Many run-time decisions are controlled by environment variables (for example `WSLG_WESTON_LOG_PATH`, `WSL2_INSTALL_PATH`, `WSLG_ERR_LOG_PATH`, `WSL2_WESTON_SHELL_DESKTOP`). When proposing code changes, reference `WSLGd/main.cpp` to see which env vars are read and where to update docs/tests.
- Logging and failure recovery: `WSLGd` intentionally restarts key subprocesses. Look for `ProcessMonitor` and `FontMonitor` in `WSLGd/` for patterns of supervising child processes.
- C++ style: project uses `wil` helpers and `THROW_LAST_ERROR_IF` macros; prefer following existing error-handling idioms in `WSLGd` code when making changes.

### Examples agents should use when editing or adding code
- To add a new env-controlled feature, update `WSLGd/main.cpp` to read the new env key, add a documented default in `README.md`/`config/BUILD.md` and add tests or run instructions in `docs/`.
- To change how the Windows plugin creates shortcuts, inspect `WSLDVCPlugin/*.cpp` and resources (.rc) in the same folder; prefer editing the VC++ project rather than hand-editing binaries.

### Useful quick commands and checks (for dev workflow automation)
- Inspect system distro processes and logs: `wsl --system <DistroName>` then `ps -ax | grep weston` and check `/mnt/wslg/weston.log` and `/mnt/wslg/stderr.log`.
- Rebuild Windows plugin (PowerShell): open solution in Visual Studio or run msbuild on `WSLDVCPlugin/WSLDVCPlugin.sln` (workspace contains a build task for `msbuild`).
- Recreate system.vhd: follow `config/BUILD.md` -> `docker build` of the image and `tar2ext4` from `hcsshim` to produce `system.vhd`.

### Do NOT assume
- Do not assume upstream components have identical APIs — the repo uses modified copies in `vendor/*-mirror`; changes often live in those forks.
- Do not assume a single unified build system: Windows build uses MSBuild/Visual Studio, system-distro build uses Docker/Meson/Make (see `container/` and `config/BUILD.md`).

### Quick checklist for PRs touching runtime
1. Point to which mirror(s) are impacted (`vendor/*-mirror`) and whether patch will be upstreamed.
2. Include how to validate change inside the system distro (which log to check, which `ps` entry to look for).
3. If Windows side is touched, include MSBuild/solution edits and any resource (.rc) changes.

If any part of this doc is unclear or you want more detail in a specific area (build scripts, debugging, or a component boundary), tell me which area and I will expand the instructions or add short examples and automation snippets.
