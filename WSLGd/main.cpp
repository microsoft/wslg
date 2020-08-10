// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precomp.h"
#include "common.h"
#include "ProcessMonitor.h"

void LogException(const char *message, const char *exceptionDescription) noexcept
{
    fprintf(stderr, "<3>WSLGd: %s %s", message ? message : "Exception:", exceptionDescription);
    return;
}

int main(int Argc, char *Argv[])
try {
    wil::g_LogExceptionCallback = LogException;

    // Open kmsg for logging errors and set it to stderr.
    //
    // N.B. fprintf must be used instead of dprintf because glibc does not correctly
    //      handle /dev/kmsg.
    {
        wil::unique_fd logFd(TEMP_FAILURE_RETRY(open(DEV_KMSG, (O_WRONLY | O_CLOEXEC))));
        if ((logFd) && (logFd.get() != STDERR_FILENO)) {
            THROW_LAST_ERROR_IF(dup3(logFd.get(), STDERR_FILENO, O_CLOEXEC) < 0);
        }
    }

    // Ensure the daemon is launched as root.
    if (geteuid() != 0) {
        LOG_ERROR("WSLGd must be run as root.");
        return 1;
    }

    // Query the VM ID.
    const char* vmId;
    THROW_ERRNO_IF(EINVAL, ((vmId = getenv("WSL2_VM_ID")) == nullptr));

    // Create a process monitor to track child processes
    wslgd::ProcessMonitor monitor(USERNAME);
    auto passwordEntry = monitor.GetUserInfo();

    // Make directories and ensure the correct permissions.
    std::filesystem::create_directories(X11_RUNTIME_DIR);
    THROW_LAST_ERROR_IF(chmod(X11_RUNTIME_DIR, 0777) < 0);
    std::filesystem::create_directories(XDG_RUNTIME_DIR);
    THROW_LAST_ERROR_IF(chmod(XDG_RUNTIME_DIR, 0777) < 0);
    THROW_LAST_ERROR_IF(chown(XDG_RUNTIME_DIR, passwordEntry->pw_uid, passwordEntry->pw_gid) < 0);

    // Set required environment variables.
    struct envVar{ const char* name; const char* value; };
    envVar variables[] = {
        {"PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games"},
        {"XDG_RUNTIME_DIR", XDG_RUNTIME_DIR},
        {"WAYLAND_DISPLAY", "wayland-0"},
        {"DISPLAY", ":0"},
        {"XCURSOR_PATH", "/usr/share/icons"},
        {"XCURSOR_THEME", "whiteglass"},
        {"XCURSOR_SIZE", "16"},
        {"PULSE_AUDIO_RDP_SINK", SHARE_PATH "/PulseAudioRDPSink"},
        {"PULSE_AUDIO_RDP_SOURCE", SHARE_PATH "/PulseAudioRDPSource"},
        {"USE_VSOCK", "1"}
    };

    for (auto &var : variables) {
        THROW_LAST_ERROR_IF(setenv(var.name, var.value, true) < 0);
    }

    // Launch weston, pulseaudio, and mstsc.exe. They will be re-launched if they exit.
    monitor.LaunchProcess(std::vector<std::string>{
        "/usr/local/bin/weston",
        "--backend=rdp-backend.so",
        "--xwayland",
        "--port=" RDP_PORT,
        "--shell=rdprail-shell.so",
        "--log=" SHARE_PATH "/weston.log"
    });

    monitor.LaunchProcess(std::vector<std::string>{
        "/usr/local/bin/pulseaudio",
        "--load=\"module-rdp-sink sink_name=RDPSink\"",
        "--load=\"module-rdp-source source_name=RDPSource\"",
        "--load=\"module-native-protocol-unix socket=" SHARE_PATH "/PulseServer auth-anonymous=true\""
    });

    std::string remote("/v:");
    remote += vmId;
    monitor.LaunchProcess(std::vector<std::string>{
        "/mnt/c/Windows/System32/mstsc.exe",
        std::move(remote),
        "C:\\ProgramData\\Microsoft\\WSL\\wslg.rdp"
    });

    return monitor.Run();
}
CATCH_RETURN_ERRNO();