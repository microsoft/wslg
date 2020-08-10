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
        int LaunchProcess(std::vector<std::string>&& argv);
        int Run();

    private:
        std::map<int, std::vector<std::string>> m_children{};
        passwd* m_user;
    };
}