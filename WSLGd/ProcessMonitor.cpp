// Copyright (C) Microsoft Corporation. All rights reserved.
#include "ProcessMonitor.h"
#include "common.h"

wslgd::ProcessMonitor::ProcessMonitor(const char* userName)
{
    THROW_ERRNO_IF(ENOENT, !(m_user = getpwnam(userName)));
}

passwd* wslgd::ProcessMonitor::GetUserInfo() const
{
    return m_user;
}

int wslgd::ProcessMonitor::LaunchProcess(std::vector<std::string>&& argv)
{
    int childPid;
    THROW_LAST_ERROR_IF((childPid = fork()) < 0);

    if (childPid == 0) try {
        // Construct a null-terminated argument array.
        std::vector<const char*> arguments;
        for (auto &arg : argv) {
            arguments.push_back(arg.c_str());
        }

        arguments.push_back(nullptr);

        // Run the process as the specified user.
        THROW_LAST_ERROR_IF(setgid(m_user->pw_gid) < 0);
        THROW_LAST_ERROR_IF(initgroups(m_user->pw_name, m_user->pw_gid) < 0);
        THROW_LAST_ERROR_IF(setuid(m_user->pw_uid) < 0);
        THROW_LAST_ERROR_IF(chdir(m_user->pw_dir) < 0);
        THROW_LAST_ERROR_IF(execvp(arguments[0], const_cast<char *const *>(arguments.data())) < 0);
    }
    CATCH_LOG();

    // Ensure that the child process exits.
    if (childPid == 0) {
        _exit(1);
    }

    m_children[childPid] = std::move(argv);
    return childPid;
}

int wslgd::ProcessMonitor::Run() try {
    // Configure a signalfd to track when child processes exit.
    sigset_t SignalMask;
    sigemptyset(&SignalMask);
    sigaddset(&SignalMask, SIGCHLD);
    THROW_LAST_ERROR_IF(sigprocmask(SIG_BLOCK, &SignalMask, nullptr) < 0);

    wil::unique_fd signalFd{signalfd(-1, &SignalMask, SFD_CLOEXEC)};
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

                auto found = m_children.find(pid);
                if (found != m_children.end()) {
                    if (!found->second.empty()) {
                        if (WIFEXITED(status)) {
                            LOG_INFO("%s exited with status %d.", found->second[0].c_str(), WEXITSTATUS(status));

                        } else if (WIFSIGNALED(status)) {
                            LOG_INFO("%s terminated with signal %d.", found->second[0].c_str(), WTERMSIG(status));
                        }

                        LaunchProcess(std::move(found->second));
                    }

                    m_children.erase(found);

                } else {
                    LOG_INFO("untracked pid %d exited with status 0x%x.", pid, status);
                }
            }
        }
    }

    return 0;
}
CATCH_RETURN_ERRNO();