#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <filesystem>
#include <map>
#include <new>
#include <vector>
#include "lxwil.h"

#define DEV_KMSG "/dev/kmsg"
#define ETH0 "eth0"
#define LOG_ERROR(str, ...) { dprintf(g_logFd, "<3>WSLGd: %s:%u: " str "\n",__FUNCTION__, __LINE__, ##__VA_ARGS__); }
#define LOG_INFO(str, ...) { dprintf(g_logFd, "<4>WSLGd: " str "\n", ##__VA_ARGS__); }
#define RDP_PORT "3391"
#define SHARE_PATH "/mnt/wsl"
#define USERNAME "wslg"
#define X11_RUNTIME_DIR SHARE_PATH "/.X11-unix"
#define XDG_RUNTIME_DIR SHARE_PATH "/xdg-runtime-dir"

static int g_logFd = STDERR_FILENO;

int LaunchProcess(passwd* passwordEntry, const std::vector<std::string>& Argv)
{
    int childPid;
    THROW_LAST_ERROR_IF((childPid = fork()) < 0);

    if (childPid == 0) try {
        // Construct a null-terminated argument array.
        std::vector<const char*> arguments;
        for (auto &arg : Argv) {
            arguments.push_back(arg.c_str());
        }

        arguments.push_back(nullptr);

        // Run the process as the specified user.
        if (passwordEntry) {
            THROW_LAST_ERROR_IF(setgid(passwordEntry->pw_gid) < 0);
            THROW_LAST_ERROR_IF(initgroups(passwordEntry->pw_name, passwordEntry->pw_gid) < 0);
            THROW_LAST_ERROR_IF(setuid(passwordEntry->pw_uid) < 0);
            THROW_LAST_ERROR_IF(chdir(passwordEntry->pw_dir) < 0);
        }

        THROW_LAST_ERROR_IF(execvp(arguments[0], const_cast<char *const *>(arguments.data())) < 0);
    }
    CATCH_LOG();

    // Ensure that the child process exits.
    if (childPid == 0) {
        _exit(1);
    }

    return childPid;
}

void LogException(const char *message, const char *exceptionDescription) noexcept
{
    if (message) {
        dprintf(g_logFd, "<3>WSLGd: %s %s", message , exceptionDescription);

    } else {
        dprintf(g_logFd, "<3>WSLGd: Exception: %s", exceptionDescription);
    }

    return;
}

int main(int Argc, char *Argv[])
try {
    wil::g_LogExceptionCallback = LogException;

    // Open kmsg for logging errors.
    g_logFd = TEMP_FAILURE_RETRY(open(DEV_KMSG, (O_WRONLY | O_CLOEXEC)));
    if (g_logFd < 0) {
        g_logFd = STDERR_FILENO;
        LOG_ERROR("open(%s) failed %d", DEV_KMSG, errno);
    }

    // Ensure the daemon is launched as root.
    if (geteuid() != 0) {
        LOG_ERROR("WSLGd must be run as root.");
        return 1;
    }

    // Get the WSLG user.
    auto passwordEntry = getpwnam(USERNAME);
    if (!passwordEntry) {
        LOG_ERROR("getpwnam(%s) failed, using root.", USERNAME);
        THROW_ERRNO_IF(ENOENT, ((passwordEntry = getpwuid(0)) == nullptr));
    }

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

    // Begin launching processes and adding them to a list of processes that will
    // be re-launched if they exit.
    //
    // TODO: Create a helper class for launching processes and monitoring their status.
    //       This can contain retry logic etc.
    std::map<int, std::vector<std::string>> children{};

    // Launch weston.
    std::string config("--config=");
    config += passwordEntry->pw_dir;
    config += "/.config/weston.ini";
    std::vector<std::string> arguments{
        "/usr/local/bin/weston",
        "--backend=rdp-backend.so",
        "--xwayland",
        "--port=" RDP_PORT,
        "--shell=rdprail-shell.so",
        std::move(config),
        "--log=" SHARE_PATH "/weston.log"
    };

    auto childPid = LaunchProcess(passwordEntry, arguments);
    children[childPid] = std::move(arguments);

    // Launch PulseAudio.
    arguments.clear();
    arguments.push_back("/usr/local/bin/pulseaudio");
    arguments.push_back("--load=\"module-rdp-sink sink_name=RDPSink\"");
    arguments.push_back("--load=\"module-rdp-source source_name=RDPSource\"");
    arguments.push_back("--load=\"module-native-protocol-unix socket=" SHARE_PATH "/PulseServer auth-anonymous=true\"");
    childPid = LaunchProcess(passwordEntry, arguments);
    children[childPid] = std::move(arguments);

    // Launch mstsc.exe.
    const char* vmId;
    THROW_ERRNO_IF(EINVAL, ((vmId = getenv("WSL2_VM_ID")) == nullptr));

    std::string remote("/v:");
    remote += vmId;

    arguments.clear();
    arguments.push_back("/mnt/c/Windows/System32/mstsc.exe");
    arguments.push_back(std::move(remote));
    arguments.push_back("C:\\ProgramData\\Microsoft\\WSL\\wslg.rdp");
    childPid = LaunchProcess(passwordEntry, arguments);
    children[childPid] = std::move(arguments);

    // Configure a signalfd to track when child processes exit.
    sigset_t SignalMask;
    sigemptyset(&SignalMask);
    sigaddset(&SignalMask, SIGCHLD);
    THROW_LAST_ERROR_IF(sigprocmask(SIG_BLOCK, &SignalMask, nullptr) < 0);

    wil::unique_fd signalFd{signalfd(-1, &SignalMask, 0)};
    THROW_LAST_ERROR_IF(!signalFd);

    // Begin monitoring loop.
    pollfd pollFd{signalFd.get(), POLLIN};
    for (;;) {
        if (poll(&pollFd, 1, -1) <= 0) {
            break;
        }

        if (pollFd.revents & POLLIN) {
            signalfd_siginfo signalInfo;
            auto bytesRead = TEMP_FAILURE_RETRY(read(pollFd.fd, &signalInfo, sizeof(signalInfo)));
            THROW_INVALID_IF(bytesRead != sizeof(signalInfo));
            THROW_INVALID_IF(signalInfo.ssi_signo != SIGCHLD);

            // Reap any zombie child processes and re-launch any tracked processes.
            for (;;) {
                int pid;
                int status;
                THROW_LAST_ERROR_IF((pid = waitpid(-1, &status, WNOHANG)) < 0);

                if (pid == 0) {
                    break;
                }

                auto found = children.find(pid);
                if (found != children.end()) {
                    if (!found->second.empty()) {
                        if (WIFEXITED(status)) {
                            LOG_INFO("%s exited with status %d.", found->second[0].c_str(), WEXITSTATUS(status));

                        } else if (WIFSIGNALED(status)) {
                            LOG_INFO("%s terminated with signal %d.", found->second[0].c_str(), WTERMSIG(status));
                        }

                        childPid = LaunchProcess(passwordEntry, found->second);
                        LOG_INFO("re-launched %s as pid %d.", found->second[0].c_str(), childPid);
                        children[childPid] = std::move(found->second);
                    }

                    children.erase(found);

                } else {
                    LOG_INFO("untracked pid %d exited with status 0x%x.", pid, status);
                }
            }
        }
    }

    return 0;
}
CATCH_RETURN_ERRNO();