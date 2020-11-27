// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precomp.h"
#include "common.h"
#include "ProcessMonitor.h"

#define SHARE_PATH "/mnt/wslg"

constexpr auto c_serviceIdTemplate = "%08X-FACB-11E6-BD58-64006A7986D3";
constexpr auto c_userName = "wslg";
constexpr auto c_vmIdEnv = "WSL2_VM_ID";

constexpr auto c_dbusDir = "/var/run/dbus";
constexpr auto c_launchPulse = "/home/wslg/launch_pulse.sh";
constexpr auto c_versionFile = "/etc/versions.txt";
constexpr auto c_versionMount = SHARE_PATH "/versions.txt";
constexpr auto c_x11RuntimeDir = SHARE_PATH "/.X11-unix";
constexpr auto c_Xauthority = SHARE_PATH "/.Xauthority";
constexpr auto c_xdgRuntimeDir = SHARE_PATH "/runtime-dir";

void LogException(const char *message, const char *exceptionDescription) noexcept
{
    fprintf(stderr, "<3>WSLGd: %s %s", message ? message : "Exception:", exceptionDescription);
    return;
}

std::string ToServiceId(unsigned int port)
{
    int size;
    THROW_LAST_ERROR_IF((size = snprintf(nullptr, 0, c_serviceIdTemplate, port)) < 0);

    std::string serviceId(size + 1, '\0');
    THROW_LAST_ERROR_IF(snprintf(&serviceId[0], serviceId.size(), c_serviceIdTemplate, port) < 0);

    return serviceId;
}

int GenerateXauthority(passwd* passwordEntry)
try {

    // If .Xauthority already exist, delete it as we'll need to re-generate it and use
    // the presence of the file to determine once we have successfully connected to a new
    // instance of the X-Server.
    if (!access(c_Xauthority, F_OK)) {
        remove(c_Xauthority);
    }

    // Ask the X-Server to write the secret connection cookie to .Xauthority to enable all WSL users
    // accounts and root to connect to it.
    int retryCount = 50;
    while (retryCount > 0) {
        int xAuthPid = fork();
        if (xAuthPid == 0) {
            // Need to run as wslg user.
            THROW_LAST_ERROR_IF(setgid(passwordEntry->pw_gid) < 0);
            THROW_LAST_ERROR_IF(initgroups(passwordEntry->pw_name, passwordEntry->pw_gid) < 0);
            THROW_LAST_ERROR_IF(setuid(passwordEntry->pw_uid) < 0);

            std::vector<const char*> args = { "/usr/bin/xauth", "-f", "/mnt/wslg/.Xauthority", "generate", ":0", ".", "trusted", "timeout", "0", NULL };
            THROW_LAST_ERROR_IF(execv(args[0], const_cast<char *const *>(args.data())) < 0);
        }

        waitpid(xAuthPid, NULL, 0);
        if (!access(c_Xauthority, F_OK)) {
            // The file exist, we can move on.
            break;
        }

        retryCount--;
        LOG_INFO("Authority file (%s) doesn't exist yet, sleeping 100ms.", c_Xauthority);
        usleep(100*1000);
    };

    if (retryCount > 0) {
        // Make .Xauthority readable by all users.
        THROW_LAST_ERROR_IF(chmod(c_Xauthority, 0644) < 0);
    } else {
        LOG_ERROR("Fail to generate %s", c_Xauthority);
    }

    return 0;
}
CATCH_RETURN_ERRNO();

int main(int Argc, char *Argv[])
try {
    wil::g_LogExceptionCallback = LogException;

    // Open kmsg for logging errors and set it to stderr.
    //
    // N.B. fprintf must be used instead of dprintf because glibc does not correctly
    //      handle /dev/kmsg.
    {
        wil::unique_fd logFd(TEMP_FAILURE_RETRY(open("/dev/kmsg", (O_WRONLY | O_CLOEXEC))));
        if ((logFd) && (logFd.get() != STDERR_FILENO)) {
            THROW_LAST_ERROR_IF(dup3(logFd.get(), STDERR_FILENO, O_CLOEXEC) < 0);
        }
    }

    // Ensure the daemon is launched as root.
    if (geteuid() != 0) {
        LOG_ERROR("must be run as root.");
        return 1;
    }

    // Query the VM ID.
    auto vmId = getenv(c_vmIdEnv);
    if (!vmId) {
        LOG_ERROR("%s must be set.", c_vmIdEnv);
        return 1;
    }

    {
        wil::unique_fd fd(open(c_versionMount, (O_RDWR | O_CREAT), (S_IRUSR | S_IRGRP | S_IROTH)));
        THROW_LAST_ERROR_IF(!fd);
    }

    THROW_LAST_ERROR_IF(mount(c_versionFile, c_versionMount, NULL, MS_BIND | MS_RDONLY, NULL) < 0);

    // Create a process monitor to track child processes
    wslgd::ProcessMonitor monitor(c_userName);
    auto passwordEntry = monitor.GetUserInfo();

    // Make directories and ensure the correct permissions.
    std::filesystem::create_directories(c_dbusDir);
    THROW_LAST_ERROR_IF(chown(c_dbusDir, passwordEntry->pw_uid, passwordEntry->pw_gid) < 0);
    THROW_LAST_ERROR_IF(chmod(c_dbusDir, 0777) < 0);

    std::filesystem::create_directories(c_x11RuntimeDir);
    THROW_LAST_ERROR_IF(chmod(c_x11RuntimeDir, 0777) < 0);

    std::filesystem::create_directories(c_xdgRuntimeDir);
    THROW_LAST_ERROR_IF(chmod(c_xdgRuntimeDir, 0700) < 0);
    THROW_LAST_ERROR_IF(chown(c_xdgRuntimeDir, passwordEntry->pw_uid, passwordEntry->pw_gid) < 0);

    // Create a listening vsock to be used for the RDP connection.
    //
    // N.B. getsockname is used to get the port assigned by the kernel.
    sockaddr_vm address{};
    address.svm_family = AF_VSOCK;
    address.svm_cid = VMADDR_CID_ANY;
    address.svm_port = VMADDR_PORT_ANY;
    socklen_t addressSize = sizeof(address);
    wil::unique_fd socketFd{socket(AF_VSOCK, SOCK_STREAM, 0)};
    THROW_LAST_ERROR_IF(!socketFd);
    auto socketFdString = std::to_string(socketFd.get());
    THROW_LAST_ERROR_IF(bind(socketFd.get(), reinterpret_cast<const sockaddr*>(&address), addressSize) < 0);
    THROW_LAST_ERROR_IF(listen(socketFd.get(), 1) < 0);
    THROW_LAST_ERROR_IF(getsockname(socketFd.get(), reinterpret_cast<sockaddr*>(&address), &addressSize));

    // Set required environment variables.
    struct envVar{ const char* name; const char* value; };
    envVar variables[] = {
        {"HOME", passwordEntry->pw_dir},
        {"USER", passwordEntry->pw_name},
        {"LOGNAME", passwordEntry->pw_name},
        {"SHELL", passwordEntry->pw_shell},
        {"PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games"},
        {"XDG_RUNTIME_DIR", c_xdgRuntimeDir},
        {"WAYLAND_DISPLAY", "wayland-0"},
        {"DISPLAY", ":0"},
        {"XCURSOR_PATH", "/usr/share/icons"},
        {"XCURSOR_THEME", "whiteglass"},
        {"XCURSOR_SIZE", "16"},
        {"PULSE_AUDIO_RDP_SINK", SHARE_PATH "/PulseAudioRDPSink"},
        {"PULSE_AUDIO_RDP_SOURCE", SHARE_PATH "/PulseAudioRDPSource"},
        {"USE_VSOCK", socketFdString.c_str()},
        {"WSL2_DEFAULT_APP_ICON", "/usr/share/icons/wsl/linux.png"},
        {"WESTON_DISABLE_ABSTRACT_FD", "1"}
    };

    for (auto &var : variables) {
        THROW_LAST_ERROR_IF(setenv(var.name, var.value, true) < 0);
    }

    // Launch weston.
    // N.B. Additional capabilities are needed to setns to the mount namespace of the user distro.
    monitor.LaunchProcess(std::vector<std::string>{
        "/usr/local/bin/weston",
        "--backend=rdp-backend.so",
        "--xwayland",
        "--shell=rdprail-shell.so",
        "--logger-scopes=rdp-backend,rdprail-shell,log",
        "--log=" SHARE_PATH "/weston.log"
        },
        std::vector<cap_value_t>{CAP_SYS_ADMIN, CAP_SYS_CHROOT, CAP_SYS_PTRACE},
        GenerateXauthority
    );

    // Launch the mstsc client.
    std::string remote("/v:");
    remote += vmId;
    std::string serviceId("/hvsocketserviceid:");
    serviceId += ToServiceId(address.svm_port);
    monitor.LaunchProcess(std::vector<std::string>{
        "/mnt/c/Windows/System32/mstsc.exe",
        std::move(remote),
        std::move(serviceId),
        "/silent",
        "C:\\ProgramData\\Microsoft\\WSL\\wslg.rdp"
    });

    // Launch the system dbus daemon.
    monitor.LaunchProcess(std::vector<std::string>{
        "/usr/bin/dbus-daemon",
        "--syslog",
        "--nofork",
        "--nopidfile",
        "--system"
    });

    // Launch pulseaudio and the associated dbus daemon.
    monitor.LaunchProcess(std::vector<std::string>{
        "/usr/bin/sh",
        "-c",
        "/usr/bin/dbus-launch "
        "/usr/local/bin/pulseaudio "
        "--log-target=file:" SHARE_PATH "/pulseaudio.log "
        "--load=\"module-rdp-sink sink_name=RDPSink\" "
        "--load=\"module-rdp-source source_name=RDPSource\" "
        "--load=\"module-native-protocol-unix socket=" SHARE_PATH "/PulseServer auth-anonymous=true\""
    });

    return monitor.Run();
}
CATCH_RETURN_ERRNO();
