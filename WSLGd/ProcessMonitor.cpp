// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
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

extern char **environ;

int wslgd::ProcessMonitor::LaunchProcess(
    std::vector<std::string>&& argv,
    std::vector<cap_value_t>&& capabilities,
    std::vector<std::string>&& env)
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

        // If any capabilities were specified, set the keepcaps flag so they are not lost across setuid.
        if (!capabilities.empty()) {
            THROW_LAST_ERROR_IF(prctl(PR_SET_KEEPCAPS, 1) < 0);
        }

        // Construct a null-terminated environment array.
        std::vector<const char*> environments;
        for (char **c = environ; *c; c++) {
            environments.push_back(*c);
        }
        for (auto &s : env) {
            if (s.size()) {
                environments.push_back(s.c_str());
            }
        }

        environments.push_back(nullptr);

        // Set user settings.
        THROW_LAST_ERROR_IF(setgid(m_user->pw_gid) < 0);
        THROW_LAST_ERROR_IF(initgroups(m_user->pw_name, m_user->pw_gid) < 0);
        THROW_LAST_ERROR_IF(setuid(m_user->pw_uid) < 0);
        THROW_LAST_ERROR_IF(chdir(m_user->pw_dir) < 0);

        // Apply additional capabilities to the process.
        if (!capabilities.empty()) {
            cap_t caps{};
            THROW_LAST_ERROR_IF((caps = cap_get_proc()) == NULL);
            auto freeCapabilities = wil::scope_exit([&caps]() { cap_free(caps); });
            THROW_LAST_ERROR_IF(cap_set_flag(caps, CAP_PERMITTED, capabilities.size(), capabilities.data(), CAP_SET) < 0);
            THROW_LAST_ERROR_IF(cap_set_flag(caps, CAP_EFFECTIVE, capabilities.size(), capabilities.data(), CAP_SET) < 0);
            THROW_LAST_ERROR_IF(cap_set_flag(caps, CAP_INHERITABLE, capabilities.size(), capabilities.data(), CAP_SET) < 0);
            THROW_LAST_ERROR_IF(cap_set_proc(caps) < 0);
            for (auto &cap : capabilities) {
                THROW_LAST_ERROR_IF(cap_set_ambient(cap, CAP_SET) < 0);
            }
        }

        // Run the process as the specified user.
        THROW_LAST_ERROR_IF(execvpe(arguments[0], const_cast<char *const *>(arguments.data()), const_cast<char *const *>(environments.data())) < 0);
    }
    CATCH_LOG();

    // Ensure that the child process exits.
    if (childPid == 0) {
        _exit(1);
    }

    m_children[childPid] = ProcessInfo{std::move(argv), std::move(capabilities), std::move(env)};
    return childPid;
}

int wslgd::ProcessMonitor::Run() try {
    std::map<std::string, std::vector<time_t>> crashes;

    for (;;) {
        // Reap any zombie child processes and re-launch any tracked processes.
        int pid;
        int status;

        /* monitor only processes within same group as caller */
        THROW_LAST_ERROR_IF((pid = waitpid(0, &status, 0)) <= 0);

        auto found = m_children.find(pid);
        if (found != m_children.end()) {
            if (!found->second.argv.empty()) {
                std::string cmd;
                for (auto &arg : found->second.argv) {
                    cmd += arg.c_str();
                    cmd += " ";
                }

                if (WIFEXITED(status)) {
                    LOG_INFO("pid %d exited with status %d, %s", pid, WEXITSTATUS(status), cmd.c_str());
                } else if (WIFSIGNALED(status)) {
                    LOG_INFO("pid %d terminated with signal %d, %s", pid, WTERMSIG(status), cmd.c_str());
                } else {
                    LOG_ERROR("pid %d return unknown status %d, %s", pid, status, cmd.c_str());
                }

                auto& crashTimestamps = crashes[cmd];
                auto now = time(nullptr);
                crashTimestamps.erase(std::remove_if(crashTimestamps.begin(), crashTimestamps.end(), [&](auto ts) { return ts < now - 60; }), crashTimestamps.end());
                crashTimestamps.emplace_back(now);

                if (crashTimestamps.size() > 10) {
                    LOG_INFO("%s exited more than 10 times in 60 seconds, not starting it again", cmd.c_str());
                } else {
                    LaunchProcess(std::move(found->second.argv), std::move(found->second.capabilities), std::move(found->second.env));
                }
            }

            m_children.erase(found);

        } else {
            LOG_INFO("untracked pid %d exited with status 0x%x.", pid, status);
        }
    }

    return 0;
}
CATCH_RETURN_ERRNO();
