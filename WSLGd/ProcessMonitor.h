// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include "precomp.h"

namespace wslgd
{
    class ProcessMonitor
    {
    public:
        ProcessMonitor(const char* username);
        ProcessMonitor(const ProcessMonitor&) = delete;
        void operator=(const ProcessMonitor&) = delete;

        passwd* GetUserInfo() const;
        typedef int (*ProcessLaunchCallack) (passwd* passwordEntry);
        int LaunchProcess(std::vector<std::string>&& argv, std::vector<cap_value_t>&& capabilities = {}, ProcessLaunchCallack callback = nullptr);
        int Run();

    private:
        struct ProcessInfo
        {
            std::vector<std::string> argv;
            std::vector<cap_value_t> capabilities;
            ProcessLaunchCallack callback;
        };

        std::map<int, ProcessInfo> m_children{};
        passwd* m_user;
    };
}