// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include "precomp.h"
#include "common.h"
#include "ProcessMonitor.h"
#include "FontMonitor.h"

#define CONFIG_FILE ".wslgconfig"
#define MSRDC_EXE "msrdc.exe"
#define MSTSC_EXE "mstsc.exe"
#define GDBSERVER_PATH "/usr/bin/gdbserver"
#define WESTON_NOTIFY_SOCKET SHARE_PATH "/weston-notify.sock"
#define DEFAULT_ICON_PATH "/usr/share/icons"
#define USER_DISTRO_ICON_PATH USER_DISTRO_MOUNT_PATH DEFAULT_ICON_PATH
#define MAX_RESERVED_PORT 1024

constexpr auto c_serviceIdTemplate = "%08X-FACB-11E6-BD58-64006A7986D3";
constexpr auto c_userName = "wslg";
constexpr auto c_vmIdEnv = "WSL2_VM_ID";

constexpr auto c_dbusDir = "/var/run/dbus";
constexpr auto c_versionFile = "/etc/versions.txt";
constexpr auto c_versionMount = SHARE_PATH "/versions.txt";
constexpr auto c_shareDocsDir = "/usr/share/doc";
constexpr auto c_shareDocsMount = SHARE_PATH "/doc";
constexpr auto c_x11RuntimeDir = SHARE_PATH "/.X11-unix";
constexpr auto c_xdgRuntimeDir = SHARE_PATH "/runtime-dir";
constexpr auto c_stdErrLogFile = SHARE_PATH "/stderr.log";

constexpr auto c_sharedMemoryMountPoint = "/mnt/shared_memory";
constexpr auto c_sharedMemoryMountPointEnv = "WSL2_SHARED_MEMORY_MOUNT_POINT";
constexpr auto c_sharedMemoryObDirectoryPathEnv = "WSL2_SHARED_MEMORY_OB_DIRECTORY";

constexpr auto c_executionAliasPathEnv = "WSL2_EXECUTION_ALIAS_PATH";
constexpr auto c_installPathEnv = "WSL2_INSTALL_PATH";
constexpr auto c_userProfileEnv = "WSL2_USER_PROFILE";
constexpr auto c_systemDistroEnvSection = "system-distro-env";

constexpr auto c_windowsSystem32 = "/mnt/c/Windows/System32";

constexpr auto c_westonShellOverrideEnv = "WSL2_WESTON_SHELL_OVERRIDE";
constexpr auto c_westonRdprailShell = "rdprail-shell";

constexpr auto c_rdpFileOverrideEnv = "WSL2_RDP_CONFIG_OVERRIDE";
constexpr auto c_rdpFile = "wslg.rdp";

void LogPrint(int level, const char *func, int line, const char *fmt, ...) noexcept
{
    std::array<char, 128> buffer;
    struct timeval tv;
    struct tm *time;
    va_list va_args;

    gettimeofday(&tv, NULL);
    time = localtime(&tv.tv_sec);
    strftime(buffer.data(), buffer.size(), "%H:%M:%S", time);
    fprintf(stderr, "[%s.%03ld] <%d>WSLGd: %s:%u: ",
        buffer.data(), (tv.tv_usec / 1000),
        level, func, line);

    va_start(va_args, fmt);
    vfprintf(stderr, fmt, va_args);
    va_end(va_args);
    fprintf(stderr, "\n");

    return;
}

void LogException(const char *message, const char *exceptionDescription) noexcept
{
    LogPrint(LOG_LEVEL_EXCEPTION, __FUNCTION__, __LINE__, "%s %s", message ? message : "Exception:", exceptionDescription);
    return;
}

bool IsNumeric(char *str)
{
    char* p;
    if (!str)
        return false;
    strtol(str, &p, 10);
    return *p == 0;
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
    std::string commandLine = "/usr/bin/wslpath -a \"";
    commandLine += Path;
    commandLine += "\"";
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

bool GetEnvBool(const char *EnvName, bool DefaultValue)
{
    char *s;

    s = getenv(EnvName);
    if (s) {
        if (strcmp(s, "true") == 0)
            return true;
        else if (strcmp(s, "false") == 0)
            return false;
        else if (strcmp(s, "1") == 0)
            return true;
        else if (strcmp(s, "0") == 0)
            return false;
    }

    return DefaultValue;
}

void SetupOptionalEnv()
{
#if HAVE_WINPR
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
#endif // HAVE_WINPR

    return;
}

int SetupReadyNotify(const char *socket_path)
{
    struct sockaddr_un addr = {};
    socklen_t size, name_size;

    addr.sun_family = AF_LOCAL;
    name_size = snprintf(addr.sun_path, sizeof addr.sun_path,
                         "%s", socket_path) + 1;
    size = offsetof(struct sockaddr_un, sun_path) + name_size;
    unlink(addr.sun_path);

    wil::unique_fd socketFd{socket(PF_LOCAL, SOCK_SEQPACKET, 0)};
    THROW_LAST_ERROR_IF(!socketFd);

    THROW_LAST_ERROR_IF(bind(socketFd.get(), reinterpret_cast<const sockaddr*>(&addr), size) < 0);
    THROW_LAST_ERROR_IF(listen(socketFd.get(), 1) < 0);

    return socketFd.release();
}

void WaitForReadyNotify(int notifyFd)
{
    // wait under client connects */
    wil::unique_fd fd(accept(notifyFd, 0, 0));
    THROW_LAST_ERROR_IF(!fd);
}

int main(int Argc, char *Argv[])
try {
    wil::g_LogExceptionCallback = LogException;

    // Restore default processing for SIGCHLD as both WSLGd and Xwayland depends on this.
    signal(SIGCHLD, SIG_DFL);

    // Create a process monitor to track child processes
    wslgd::ProcessMonitor monitor(c_userName);
    auto passwordEntry = monitor.GetUserInfo();

    // Set required environment variables.
    struct envVar{ const char* name; const char* value; bool override; };
    envVar variables[] = {
        {"HOME", passwordEntry->pw_dir, true},
        {"USER", passwordEntry->pw_name, true},
        {"LOGNAME", passwordEntry->pw_name, true},
        {"SHELL", passwordEntry->pw_shell, true},
        {"PATH", "/usr/sbin:/usr/bin:/sbin:/bin:/usr/games", true},
        {"XDG_RUNTIME_DIR", c_xdgRuntimeDir, false},
        {"WAYLAND_DISPLAY", "wayland-0", false},
        {"DISPLAY", ":0", false},
        {"XCURSOR_PATH", USER_DISTRO_ICON_PATH ":" DEFAULT_ICON_PATH , false},
        {"XCURSOR_THEME", "whiteglass", false},
        {"XCURSOR_SIZE", "16", false},
        {"PULSE_SERVER", SHARE_PATH "/PulseServer", false},
        {"PULSE_AUDIO_RDP_SINK", SHARE_PATH "/PulseAudioRDPSink", false},
        {"PULSE_AUDIO_RDP_SOURCE", SHARE_PATH "/PulseAudioRDPSource", false},
        {"WSL2_DEFAULT_APP_ICON", DEFAULT_ICON_PATH "/wsl/linux.png", false},
        {"WSL2_DEFAULT_APP_OVERLAY_ICON", DEFAULT_ICON_PATH "/wsl/linux.png", false},
    };

    for (auto &var : variables) {
        THROW_LAST_ERROR_IF(setenv(var.name, var.value, var.override) < 0);
    }

    SetupOptionalEnv();

    // if any components output log to /dev/kmsg, make it writable.
    if (GetEnvBool("WSLG_LOG_KMSG", false))
        THROW_LAST_ERROR_IF(chmod("/dev/kmsg", 0666) < 0);

    // Open a file for logging errors and set it to stderr for WSLGd as well as any child process.
    {
        const char *errLog = getenv("WSLG_ERR_LOG_PATH");
        if (!errLog) {
            errLog = c_stdErrLogFile;
        }
        wil::unique_fd stdErrLogFd(open(errLog, (O_RDWR | O_CREAT), (S_IRUSR | S_IRGRP | S_IROTH)));
        if (stdErrLogFd && (stdErrLogFd.get() != STDERR_FILENO)) {
            dup2(stdErrLogFd.get(), STDERR_FILENO);
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

    // Query the WSL install path.
    bool isWslInstallPathEnvPresent = false;
    std::string wslInstallPath = "C:\\ProgramData\\Microsoft\\WSL";
    auto installPath = getenv(c_installPathEnv);
    if (installPath) {
        isWslInstallPathEnvPresent = true;
        wslInstallPath = installPath;
    }

    std::string wslExecutionAliasPath;
    auto executionAliasPath = getenv(c_executionAliasPathEnv);
    if (executionAliasPath) {
        wslExecutionAliasPath = executionAliasPath;
    }

    // Bind mount the versions.txt file which contains version numbers of the various WSLG pieces.
    {
        wil::unique_fd fd(open(c_versionMount, (O_RDWR | O_CREAT), (S_IRUSR | S_IRGRP | S_IROTH)));
        THROW_LAST_ERROR_IF(!fd);
    }

    THROW_LAST_ERROR_IF(mount(c_versionFile, c_versionMount, NULL, MS_BIND | MS_RDONLY, NULL) < 0);

    std::filesystem::create_directories(c_shareDocsMount);
    THROW_LAST_ERROR_IF(mount(c_shareDocsDir, c_shareDocsMount, NULL, MS_BIND | MS_RDONLY, NULL) < 0);

    // Create a font folder monitor
    wslgd::FontMonitor fontMonitor;

    // Make directories and ensure the correct permissions.
    std::filesystem::create_directories(c_dbusDir);
    THROW_LAST_ERROR_IF(chown(c_dbusDir, passwordEntry->pw_uid, passwordEntry->pw_gid) < 0);
    THROW_LAST_ERROR_IF(chmod(c_dbusDir, 0777) < 0);

    std::filesystem::create_directories(c_x11RuntimeDir);
    THROW_LAST_ERROR_IF(chmod(c_x11RuntimeDir, 0777) < 0);

    std::filesystem::create_directories(c_xdgRuntimeDir);
    THROW_LAST_ERROR_IF(chown(c_xdgRuntimeDir, passwordEntry->pw_uid, passwordEntry->pw_gid) < 0);
    THROW_LAST_ERROR_IF(chmod(c_xdgRuntimeDir, 0777) < 0);

    // Attempt to mount the virtiofs share for shared memory.
    bool isSharedMemoryMounted = false; 
    auto sharedMemoryObDirectoryPath = getenv(c_sharedMemoryObDirectoryPathEnv);
    if (sharedMemoryObDirectoryPath) {
        std::filesystem::create_directories(c_sharedMemoryMountPoint);
        if (mount("wslg", c_sharedMemoryMountPoint, "virtiofs", 0, "dax") < 0) {
            LOG_ERROR("Failed to mount wslg shared memory %s.", strerror(errno));
        } else {
            THROW_LAST_ERROR_IF(chmod(c_sharedMemoryMountPoint, 0777) < 0);
            isSharedMemoryMounted = true;
        }
    } else {
        LOG_ERROR("shared memory ob directory path is not set.");
    }

    // Create a listening vsock in the reserved port range to be used for the RDP connection.
    sockaddr_vm address{};
    address.svm_family = AF_VSOCK;
    address.svm_cid = VMADDR_CID_ANY;
    socklen_t addressSize = sizeof(address);
    wil::unique_fd socketFd{socket(AF_VSOCK, SOCK_STREAM, 0)};
    THROW_LAST_ERROR_IF(!socketFd);
    for (unsigned int port = 1; port < MAX_RESERVED_PORT; port += 1) {
        address.svm_port = port;
        if (bind(socketFd.get(), reinterpret_cast<const sockaddr*>(&address), addressSize) == 0) {
            break;
        }

        THROW_LAST_ERROR_IF(errno != EADDRINUSE);
    }

    THROW_ERRNO_IF(EINVAL, (address.svm_port == MAX_RESERVED_PORT));
    THROW_LAST_ERROR_IF(listen(socketFd.get(), 1) < 0);
    std::string socketEnvString("USE_VSOCK=");
    socketEnvString += std::to_string(socketFd.get());
    std::string serviceIdEnvString("WSLG_SERVICE_ID=");
    serviceIdEnvString += ToServiceId(address.svm_port);

    struct rlimit limit;
    THROW_LAST_ERROR_IF(getrlimit(RLIMIT_NOFILE, &limit) < 0);
    limit.rlim_cur = limit.rlim_max;
    THROW_LAST_ERROR_IF(setrlimit(RLIMIT_NOFILE, &limit) < 0);

    // Set shared memory mount point to env when available.
    if (!isSharedMemoryMounted ||
        (setenv(c_sharedMemoryMountPointEnv, c_sharedMemoryMountPoint, true) < 0)) {
        // shared memory is not available, env must be cleared if it's set.
        THROW_LAST_ERROR_IF(unsetenv(c_sharedMemoryMountPointEnv) < 0);
        isSharedMemoryMounted = false;
    }

    // Construct socket option string.
    std::string westonSocketOption("--socket=");
    westonSocketOption += getenv("WAYLAND_DISPLAY");

    // Check if weston shell override is specified.
    // Otherwise, default shell is 'rdprail-shell'.
    bool isRdprailShell;
    std::string westonShellName;
    auto westonShellEnv = getenv(c_westonShellOverrideEnv);
    if (!westonShellEnv) {
        westonShellName = c_westonRdprailShell;
        isRdprailShell = true;
    } else {
        westonShellName = westonShellEnv;
        isRdprailShell = (westonShellName.compare(c_westonRdprailShell) == 0);
    }

    // Construct shell option string.
    std::string westonShellOption("--shell=");
    westonShellOption += westonShellName;
    westonShellOption += ".so";

    // Construct log file option string.
    std::string westonLogFileOption("--log=");
    auto westonLogFilePathEnv = getenv("WSLG_WESTON_LOG_PATH");
    if (westonLogFilePathEnv) {
        westonLogFileOption += westonLogFilePathEnv;
    } else {
        westonLogFileOption += SHARE_PATH "/weston.log";
    }

    // Construct logger option string.
    // By default, enable standard log and rdp-backend.
    std::string westonLoggerOption("--logger-scopes=log,rdp-backend");
    // If rdprail-shell is used, enable logger for that.
    if (isRdprailShell) {
        westonLoggerOption += ",";
        westonLoggerOption += c_westonRdprailShell;
    }

    // Setup notify for wslgd-notify.so
    wil::unique_fd notifyFd(SetupReadyNotify(WESTON_NOTIFY_SOCKET));
    THROW_LAST_ERROR_IF(!notifyFd);

    // Construct weston option string.
    std::string westonArgs;
    char *gdbServerPort = getenv("WSLG_WESTON_GDBSERVER_PORT");
    if ((access(GDBSERVER_PATH, X_OK) == 0) && IsNumeric(gdbServerPort)) {
        westonArgs += GDBSERVER_PATH;
        westonArgs += " :";
        westonArgs += gdbServerPort;
        westonArgs += " ";
    }
    westonArgs += "/usr/bin/weston ";
    westonArgs += "--backend=rdp-backend.so --modules=wslgd-notify.so --xwayland ";
    westonArgs += westonSocketOption;
    westonArgs += " ";
    westonArgs += westonShellOption;
    westonArgs += " ";
    westonArgs += westonLogFileOption;
    westonArgs += " ";
    westonArgs += westonLoggerOption;

    // Launch weston.
    // N.B. Additional capabilities are needed to setns to the mount namespace of the user distro.
    monitor.LaunchProcess(std::vector<std::string>{
                "/usr/bin/sh",
                "-c",
                std::move(westonArgs)
            },
            std::vector<cap_value_t>{
                CAP_SYS_ADMIN,
                CAP_SYS_CHROOT,
                CAP_SYS_PTRACE
            },
            std::vector<std::string>{
                std::move(socketEnvString),
                std::move(serviceIdEnvString),
                "WSLGD_NOTIFY_SOCKET=" WESTON_NOTIFY_SOCKET,
                "WESTON_DISABLE_ABSTRACT_FD=1",
                getenv("WLOG_APPENDER") ? : "", "WLOG_APPENDER=file",
                getenv("WLOG_FILEAPPENDER_OUTPUT_FILE_NAME") ? "" : "WLOG_FILEAPPENDER_OUTPUT_FILE_NAME=wlog.log",
                getenv("WLOG_FILEAPPENDER_OUTPUT_FILE_PATH") ? "" : "WLOG_FILEAPPENDER_OUTPUT_FILE_PATH=" SHARE_PATH
            }
        );

    // Wait weston to be ready before starting RDP client, pulseaudio server.
    WaitForReadyNotify(notifyFd.get());
    unlink(WESTON_NOTIFY_SOCKET);

    // Start font monitoring if user distro's X11 fonts to be shared with system distro.
    if (GetEnvBool("WSLG_USE_USER_DISTRO_XFONTS", true))
        fontMonitor.Start(); 

    // Launch the mstsc/msrdc client.
    std::string remote("/v:");
    remote += vmId;
    std::string serviceId("/hvsocketserviceid:");
    serviceId += ToServiceId(address.svm_port);
    std::string sharedMemoryObPath("");
    if (isSharedMemoryMounted) {
        sharedMemoryObPath += "/wslgsharedmemorypath:";
        sharedMemoryObPath += sharedMemoryObDirectoryPath;
    }

    std::filesystem::path rdpClientExePath;
    bool isUseMstsc = GetEnvBool("WSLG_USE_MSTSC", false);
    if (!isUseMstsc && !wslExecutionAliasPath.empty()) {
        std::filesystem::path msrdcExePath = TranslateWindowsPath(wslExecutionAliasPath.c_str());
        msrdcExePath /= MSRDC_EXE;
        if (access(msrdcExePath.c_str(), X_OK) == 0) {
            rdpClientExePath = std::move(msrdcExePath);
        }
    }
    if (rdpClientExePath.empty()) {
        rdpClientExePath = c_windowsSystem32;
        rdpClientExePath /= MSTSC_EXE;
    }

    std::string wslDvcPlugin;
    if (GetEnvBool("WSLG_USE_WSLDVC_PRIVATE", false))
        wslDvcPlugin = "/plugin:WSLDVC_PRIVATE";
    else if (isWslInstallPathEnvPresent)
        wslDvcPlugin = "/plugin:WSLDVC_PACKAGE";
    else
        wslDvcPlugin = "/plugin:WSLDVC";

    std::string rdpFilePathArg(wslInstallPath);
    auto rdpFile = getenv(c_rdpFileOverrideEnv);
    if (rdpFile) {
        if (strstr(rdpFile, "..\\") || strstr(rdpFile, "../")) {
            LOG_ERROR("RDP file must not contain relative path (%s)", rdpFile);
            rdpFile = nullptr;
        }
    }
    rdpFilePathArg += "\\"; // Windows-style path
    if (rdpFile) {
        rdpFilePathArg += rdpFile;
    } else {
        rdpFilePathArg += c_rdpFile;
    }

    monitor.LaunchProcess(std::vector<std::string>{
        "/init",
        std::move(rdpClientExePath),
        basename(rdpClientExePath.c_str()),
        "/wslg", // set wslg option first so following parameters are parsed in context of wslg.
        "/silent", // then set silent option before anything-else.
        std::move(remote),
        std::move(serviceId),
        std::move(wslDvcPlugin),
        std::move(sharedMemoryObPath),
        std::move(rdpFilePathArg)
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

    // Construct pulseaudio launch command line.
    std::string pulseaudioLaunchArgs =
        "/usr/bin/dbus-launch "
        "/usr/bin/pulseaudio "
        "--log-time=true "
        "--disallow-exit=true "
        "--exit-idle-time=-1 "
        "--load=\"module-rdp-sink sink_name=RDPSink\" "
        "--load=\"module-rdp-source source_name=RDPSource\" "
        "--load=\"module-native-protocol-unix socket=" SHARE_PATH "/PulseServer auth-anonymous=true\" ";

    // Construct log file option string.
    std::string pulseaudioLogFileOption("--log-target=");
    auto pulseAudioLogFilePathEnv = getenv("WSLG_PULSEAUDIO_LOG_PATH");
    if (pulseAudioLogFilePathEnv) {
        pulseaudioLogFileOption += pulseAudioLogFilePathEnv;
    } else {
        pulseaudioLogFileOption += "newfile:" SHARE_PATH "/pulseaudio.log";
    }
    pulseaudioLaunchArgs += pulseaudioLogFileOption;

    // Launch pulseaudio and the associated dbus daemon.
    monitor.LaunchProcess(std::vector<std::string>{
        "/usr/bin/sh",
        "-c",
        std::move(pulseaudioLaunchArgs)
    });

    return monitor.Run();
}
CATCH_RETURN_ERRNO();
