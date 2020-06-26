// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
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
        int LaunchProcess(std::vector<std::string>&& argv, std::vector<cap_value_t>&& capabilities = {});
        int Run();

    private:
        struct ProcessInfo
        {
            std::vector<std::string> argv;
            std::vector<cap_value_t> capabilities;
        };

        std::map<int, ProcessInfo> m_children{};
        passwd* m_user;
    };
}