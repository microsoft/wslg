// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once
#include "precomp.h"

namespace wslgd
{
    class FontFolder
    {
    public:
        FontFolder(int fd, const char *path);
        ~FontFolder();

        void ModifyX11FontPath(bool add);

        static bool ExecuteShellCommand(std::vector<const char*>&& argv);

        int GetFd() const { return m_fd.get(); }
        int GetWd() const { return m_wd; }

        bool IsPathAdded() const { return m_isPathAdded; }
        const char *GetPath() const { return m_path.c_str(); }

    private:
        wil::unique_fd m_fd; /* from FontMonitor's inotify_init() */
        int m_wd = -1; /* from inotify_add_watch() for this folder */
        std::string m_path; /* this folder path */
        bool m_isPathAdded = false; /* whether font path is added to X11 font path */
    };

    class FontMonitor
    {
    public:
        FontMonitor();
        ~FontMonitor() { Stop(); }

        FontMonitor(const FontMonitor&) = delete;
        void operator=(const FontMonitor&) = delete;

        int Start();
        void Stop();

        static void* FontMonitorThread(void *context);

        void AddMonitorFolder(const char *path);
        void RemoveMonitorFolder(const char *path);
        void DumpMonitorFolders();

        void HandleFolderEvent(struct inotify_event *event);
        void HandleFontsDirEvent(struct inotify_event *event);

        int GetFd() const { return m_fd.get(); }

    private:
        wil::unique_fd m_fd; /* from inotify_init() */
        std::map<std::string, std::unique_ptr<FontFolder>> m_fontMonitorFolders{};
        pthread_t m_fontMonitorThread = 0;
    };
}
