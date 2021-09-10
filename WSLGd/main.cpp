// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include "precomp.h"
#include "common.h"
#include "ProcessMonitor.h"

#define CONFIG_FILE ".wslgconfig"
#define SHARE_PATH "/mnt/wslg"
#define MSRDC_EXE "msrdc.exe"

constexpr auto c_serviceIdTemplate = "%08X-FACB-11E6-BD58-64006A7986D3";
constexpr auto c_userName = "wslg";
constexpr auto c_vmIdEnv = "WSL2_VM_ID";

constexpr auto c_dbusDir = "/var/run/dbus";
constexpr auto c_launchPulse = "/home/wslg/launch_pulse.sh";
constexpr auto c_versionFile = "/etc/versions.txt";
constexpr auto c_versionMount = SHARE_PATH "/versions.txt";
constexpr auto c_shareDocsDir = "/usr/share/doc";
constexpr auto c_shareDocsMount = SHARE_PATH "/doc";
constexpr auto c_x11RuntimeDir = SHARE_PATH "/.X11-unix";
constexpr auto c_xdgRuntimeDir = SHARE_PATH "/runtime-dir";
constexpr auto c_stdErrLogFile = SHARE_PATH "/stderr.log";

constexpr auto c_coreDir = SHARE_PATH "/dumps";
constexpr auto c_corePatternDefault = "core.%e";
constexpr auto c_corePatternFile = "/proc/sys/kernel/core_pattern";
constexpr auto c_corePatternEnv = "WSL2_WSLG_CORE_PATTERN";

constexpr auto c_sharedMemoryMountPoint = "/mnt/shared_memory";
constexpr auto c_sharedMemoryMountPointEnv = "WSL2_SHARED_MEMORY_MOUNT_POINT";
constexpr auto c_sharedMemoryObDirectoryPathEnv = "WSL2_SHARED_MEMORY_OB_DIRECTORY";

constexpr auto c_installPathEnv = "WSL2_INSTALL_PATH";
constexpr auto c_userProfileEnv = "WSL2_USER_PROFILE";
constexpr auto c_systemDistroEnvSection = "system-distro-env";

constexpr auto c_mstsc_fullpath = "/mnt/c/Windows/System32/mstsc.exe";

constexpr auto c_westonShellOverrideEnv = "WSL2_WESTON_SHELL_OVERRIDE";
constexpr auto c_westonRdprailShell = "rdprail-shell";

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

std::string TranslateWindowsPath(const char * Path)
{
    std::string commandLine = "/usr/bin/wslpath -a '";
    commandLine += Path;
    commandLine += "'";
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(commandLine.c_str(), "r"), pclose);
    THROW_LAST_ERROR_IF(!pipe);

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    /* trim '\n' from wslpath output */
    while (result.back() == '\n') {
        result.pop_back();
    }

    THROW_ERRNO_IF(EINVAL, pclose(pipe.release()) != 0);

    return result;
}

void SetupOptionalEnv()
{
#ifdef HAVE_WINPR2
    // Get the path to the WSLG config file.
    std::string configFilePath = "/mnt/c/ProgramData/Microsoft/WSL/" CONFIG_FILE;
    auto userProfile = getenv(c_userProfileEnv);
    if (userProfile) {
        configFilePath = TranslateWindowsPath(userProfile);
        configFilePath += "/" CONFIG_FILE;
    }

    // Set additional environment variables.
    wIniFile* wslgConfigIniFile = IniFile_New();
    if (wslgConfigIniFile) {
        if (IniFile_ReadFile(wslgConfigIniFile, configFilePath.c_str()) > 0) {
            int numKeys = 0;
            char **keyNames = IniFile_GetSectionKeyNames(wslgConfigIniFile, c_systemDistroEnvSection, &numKeys);
            for (int n = 0; keyNames && n < numKeys; n++) {
                const char *value = IniFile_GetKeyValueString(wslgConfigIniFile, c_systemDistroEnvSection, keyNames[n]);
                if (value) {
                    setenv(keyNames[n], value, true);
                }
            }

            free(keyNames);
        }

        IniFile_Free(wslgConfigIniFile);
    }
#endif // HAVE_WINPR2 

    return;
}

int main(int Argc, char *Argv[])
try {
    wil::g_LogExceptionCallback = LogException;

    // Open a file for logging errors and set it to stderr for WSLGd as well as any child process.
    {
        wil::unique_fd stdErrLogFd(open(c_stdErrLogFile, (O_RDWR | O_CREAT), (S_IRUSR | S_IRGRP | S_IROTH)));
        if (stdErrLogFd && (stdErrLogFd.get() != STDERR_FILENO)) {
            dup2(stdErrLogFd.get(), STDERR_FILENO);
        }
    }

    // Restore default processing for SIGCHLD as both WSLGd and Xwayland depends on this.
    signal(SIGCHLD, SIG_DFL);

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

    // Query the WSL install path.
    bool is_wsl_install_path_env_present = false;
    std::string wslInstallPath = "C:\\ProgramData\\Microsoft\\WSL";
    auto installPath = getenv(c_installPathEnv);
    if (installPath) {
        is_wsl_install_path_env_present = true;
        wslInstallPath = installPath;
    }

    // Bind mount the versions.txt file which contains version numbers of the various WSLG pieces.
    {
        wil::unique_fd fd(open(c_versionMount, (O_RDWR | O_CREAT), (S_IRUSR | S_IRGRP | S_IROTH)));
        THROW_LAST_ERROR_IF(!fd);
    }

    THROW_LAST_ERROR_IF(mount(c_versionFile, c_versionMount, NULL, MS_BIND | MS_RDONLY, NULL) < 0);

    std::filesystem::create_directories(c_shareDocsMount);
    THROW_LAST_ERROR_IF(mount(c_shareDocsDir, c_shareDocsMount, NULL, MS_BIND | MS_RDONLY, NULL) < 0);

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

    // Attempt to mount the virtiofs share for shared memory.
    bool is_shared_memory_mounted = false; 
    auto sharedMemoryObDirectoryPath = getenv(c_sharedMemoryObDirectoryPathEnv);
    if (sharedMemoryObDirectoryPath) {
        std::filesystem::create_directories(c_sharedMemoryMountPoint);
        if (mount("wslg", c_sharedMemoryMountPoint, "virtiofs", 0, "dax") < 0) {
            LOG_ERROR("Failed to mount wslg shared memory %s.", strerror(errno));
        } else {
            THROW_LAST_ERROR_IF(chmod(c_sharedMemoryMountPoint, 0777) < 0);
            is_shared_memory_mounted = true;
        }
    } else {
        LOG_ERROR("shared memory ob directory path is not set.");
    }

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
        {"PATH", "/usr/sbin:/usr/bin:/sbin:/bin:/usr/games"},
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
        {"WSL2_DEFAULT_APP_OVERLAY_ICON", "/usr/share/icons/wsl/linux.png"},
        {"WESTON_DISABLE_ABSTRACT_FD", "1"}
    };

    for (auto &var : variables) {
        THROW_LAST_ERROR_IF(setenv(var.name, var.value, true) < 0);
    }

    SetupOptionalEnv();

    // "ulimits -c unlimited" for core dumps.
    struct rlimit limit;
    limit.rlim_cur = RLIM_INFINITY;
    limit.rlim_max = RLIM_INFINITY;
    THROW_LAST_ERROR_IF(setrlimit(RLIMIT_CORE, &limit) < 0);

    // create folder to store core files.
    std::filesystem::create_directories(c_coreDir);
    THROW_LAST_ERROR_IF(chmod(c_coreDir, 0777) < 0);

    // update core_pattern.
    {
        wil::unique_file corePatternFile(fopen(c_corePatternFile, "w"));
        if (corePatternFile.get()) {
            // combine folder path and core pattern.
            std::string corePatternFullPath(c_coreDir);
            corePatternFullPath += "/";
            auto corePattern = getenv(c_corePatternEnv);
            if (corePattern) {
                corePatternFullPath += corePattern;
            } else {
                corePatternFullPath += c_corePatternDefault; // set to default core_pattern.
            }
            // write to core_pattern file.
            THROW_LAST_ERROR_IF(fprintf(corePatternFile.get(), "%s", corePatternFullPath.c_str()) < 0);
        }
    }

    // Set shared memory mount point to env when available.
    if (!is_shared_memory_mounted ||
        (setenv(c_sharedMemoryMountPointEnv, c_sharedMemoryMountPoint, true) < 0)) {
        // shared memory is not available, env must be cleared if it's set.
        THROW_LAST_ERROR_IF(unsetenv(c_sharedMemoryMountPointEnv) < 0);
        is_shared_memory_mounted = false;
    }

    // Check if weston shell override is specified.
    // Otherwise, default shell is 'rdprail-shell'.
    bool is_rdprail_shell;
    std::string weston_shell_name;
    auto weston_shell_env = getenv(c_westonShellOverrideEnv);
    if (!weston_shell_env) {
        weston_shell_name = c_westonRdprailShell;
        is_rdprail_shell = true;
    } else {
        weston_shell_name = weston_shell_env;
        is_rdprail_shell = (weston_shell_name.compare(c_westonRdprailShell) == 0);
    }

    // Construct shell option string.
    std::string weston_shell_option("--shell=");
    weston_shell_option += weston_shell_name;
    weston_shell_option += ".so";

    // Construct logger option string.
    // By default, enable standard log and rdp-backend.
    std::string weston_logger_option("--logger-scopes=log,rdp-backend");
    // If rdprail-shell is used, enable logger for that.
    if (is_rdprail_shell) {
        weston_logger_option += ",";
        weston_logger_option += c_westonRdprailShell;
    }

    // Launch weston.
    // N.B. Additional capabilities are needed to setns to the mount namespace of the user distro.
    monitor.LaunchProcess(std::vector<std::string>{
        "/usr/bin/weston",
        "--backend=rdp-backend.so",
        "--xwayland",
        std::move(weston_shell_option),
        std::move(weston_logger_option),
        "--log=" SHARE_PATH "/weston.log"
        },
        std::vector<cap_value_t>{CAP_SYS_ADMIN, CAP_SYS_CHROOT, CAP_SYS_PTRACE}
    );

    // Launch the mstsc/msrdc client.
    std::string remote("/v:");
    remote += vmId;
    std::string serviceId("/hvsocketserviceid:");
    serviceId += ToServiceId(address.svm_port);
    std::string shared_memory_ob_path("");
    if (is_shared_memory_mounted) {
        shared_memory_ob_path += "/wslgsharedmemorypath:";
        shared_memory_ob_path += sharedMemoryObDirectoryPath;
    }

    std::string rdpClientExePath = c_mstsc_fullpath;
    if (is_wsl_install_path_env_present) {
        struct stat buffer;
        std::string msrdcExePath = TranslateWindowsPath(wslInstallPath.c_str());
        msrdcExePath += "/" MSRDC_EXE;
        if (stat(msrdcExePath.c_str(), &buffer) == 0) {
            rdpClientExePath = msrdcExePath;
        }
    }
    std::string rdpFilePath = wslInstallPath + "\\wslg.rdp";
    monitor.LaunchProcess(std::vector<std::string>{
        std::move(rdpClientExePath),
        std::move(remote),
        std::move(serviceId),
        "/silent",
        "/wslg",
        "/plugin:WSLDVC",
        std::move(shared_memory_ob_path),
        std::move(rdpFilePath)
    });

    // Launch the system dbus daemon.
    monitor.LaunchProcess(std::vector<std::string>{
        "/usr/bin/dbus-daemon",
        "--syslog",
        "--nofork",
        "--nopidfile",
        "--system"
        },
        std::vector<cap_value_t>{CAP_SETGID, CAP_SETUID}
    );

    // Launch pulseaudio and the associated dbus daemon.
    monitor.LaunchProcess(std::vector<std::string>{
        "/usr/bin/sh",
        "-c",
        "/usr/bin/dbus-launch "
        "/usr/bin/pulseaudio "
        "--log-target=file:" SHARE_PATH "/pulseaudio.log "
        "--load=\"module-rdp-sink sink_name=RDPSink\" "
        "--load=\"module-rdp-source source_name=RDPSource\" "
        "--load=\"module-native-protocol-unix socket=" SHARE_PATH "/PulseServer auth-anonymous=true\""
    });

    return monitor.Run();
}
CATCH_RETURN_ERRNO();
