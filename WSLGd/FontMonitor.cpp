// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include "FontMonitor.h"
#include "common.h"

#define USER_DISTRO_MOUNT_PATH "/mnt/wslg/distro"
#define USER_DISTRO_FONTPATH USER_DISTRO_MOUNT_PATH "/usr/share/fonts/"

constexpr auto c_fontsdir = "fonts.dir";
constexpr auto c_xset = "/usr/bin/xset";

wslgd::FontFolder::FontFolder(int fd, const char *path)
{
    LOG_INFO("FontMonitor: start monitoring %s", path);

    m_fd.reset(fd);
    m_path = path;

    /* check if folder is already ready to be added to font path */
    try {
        std::string fonts_dir(path);
        fonts_dir += c_fontsdir;
        if (access(fonts_dir.c_str(), R_OK) == 0) {
            ModifyX11FontPath(true);
        }
        /* add watch for install or uninstall of fonts on this folder */
        THROW_LAST_ERROR_IF((m_wd = inotify_add_watch(m_fd.get(), path, IN_CREATE|IN_CLOSE_WRITE|IN_DELETE|IN_MOVED_TO|IN_MOVED_FROM)) < 0);
    }
    CATCH_LOG();
}

wslgd::FontFolder::~FontFolder()
{
    LOG_INFO("FontMonitor: stop monitoring %s", m_path.c_str());

    ModifyX11FontPath(false);

    /* if still under watch, remove it */
    if (m_wd >= 0) {
        inotify_rm_watch(m_fd.get(), m_wd);
        m_wd = -1;
    }

    m_fd.release(); /* do not close */
}

bool wslgd::FontFolder::ExecuteShellCommand(const char *cmd)
{
    bool success = false;
    try {
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
        THROW_LAST_ERROR_IF(!pipe);
        THROW_ERRNO_IF(EINVAL, pclose(pipe.release()) != 0);
        success = true;
    }
    CATCH_LOG();
    LOG_INFO("FontMonitor: execuate %s, %s", cmd, success ? "success" : "fail");
    return success;
}

void wslgd::FontFolder::ModifyX11FontPath(bool isAdd)
{
    try {
        if (m_isPathAdded != isAdd) {
            /* update X server font path, add or remove. */
            std::string cmd(c_xset);
            if (isAdd)
                cmd += " +fp ";
            else
                cmd += " -fp ";
            cmd += m_path;
            if (ExecuteShellCommand(cmd.c_str())) {
                m_isPathAdded = isAdd;

                /* let X server reread font database */
                sleep(2); /* workaround for optional fonts.alias, wait 2 sec to run rehash */
                std::string cmd(c_xset);
                cmd += " fp rehash";
                ExecuteShellCommand(cmd.c_str());
            }
        }
    }
    CATCH_LOG();
}

wslgd::FontMonitor::FontMonitor()
{
}

void wslgd::FontMonitor::DumpMonitorFolders()
{
    try {
        std::map<std::string, std::unique_ptr<FontFolder>>::iterator it;
        for (it = m_fontMonitorFolders.begin(); it != m_fontMonitorFolders.end(); it++)
            LOG_INFO("FontMonitor: monitoring %s, and it is %s to X11 font path", it->first.c_str(),
                it->second->IsPathAdded() ? "added" : "*not* added");
    }
    CATCH_LOG();
}

void wslgd::FontMonitor::AddMonitorFolder(const char *path)
{
    try {
        std::string monitorPath(path);
        // checkf if path is tracked already.
        if (m_fontMonitorFolders.find(monitorPath) == m_fontMonitorFolders.end()) {
            std::unique_ptr<FontFolder> fontFolder(new FontFolder(m_fd.get(), path));
            if (fontFolder.get()->GetWd() >= 0) {
                m_fontMonitorFolders.insert(std::make_pair(std::move(monitorPath), std::move(fontFolder)));
                // If this is mount path, only track under X11 folder if it's already exist.
                if (strcmp(path, USER_DISTRO_FONTPATH) == 0) {
                    if (std::filesystem::exists(USER_DISTRO_FONTPATH "X11/")) {
                        AddMonitorFolder(USER_DISTRO_FONTPATH "X11/");
                    }
                } else {
                    // Otherwise, add all existing subfolders to track.
                    for (auto& dir_entry : std::filesystem::directory_iterator{path}) {
                        if (dir_entry.is_directory()) {
                            std::string dirEntry(dir_entry.path().c_str());
                            dirEntry += "/";
                            AddMonitorFolder(dirEntry.c_str());
                        }
                    }
                }
            }
        } else {
            LOG_INFO("FontMonitor: %s is already tracked", path);
        }
    }
    CATCH_LOG();
}

void wslgd::FontMonitor::RemoveMonitorFolder(const char *path)
{
    try {
        std::string monitorPath(path);
        m_fontMonitorFolders.erase(monitorPath);
    }
    CATCH_LOG();
}

void wslgd::FontMonitor::HandleFolderEvent(struct inotify_event *event)
{
    try {
        std::map<std::string, std::unique_ptr<FontFolder>>::iterator it;
        for (it = m_fontMonitorFolders.begin(); it != m_fontMonitorFolders.end(); it++) {
            if (event->wd == it->second->GetWd()) {
                if (event->mask & (IN_CREATE|IN_MOVED_TO)) {
                    bool addMonitorFolder = true;
                    std::string fullPath(it->second->GetPath());
                    if (fullPath.compare(USER_DISTRO_FONTPATH) == 0) {
                        /* Immediately under mount folder, only monitor "X11" and its subfolder */
                        addMonitorFolder = (strcmp(event->name, "X11") == 0);
                    }
                    if (addMonitorFolder) {
                        fullPath += event->name;
                        fullPath += "/";
                        AddMonitorFolder(fullPath.c_str());
                    }
                } else if (event->mask & (IN_DELETE|IN_MOVED_FROM)) {
                    std::string fullPath(it->second->GetPath());
                    fullPath += event->name;
                    fullPath += "/";
                    RemoveMonitorFolder(fullPath.c_str());
                }
                break;
            }
        }
    }
    CATCH_LOG();
}

void wslgd::FontMonitor::HandleFontsDirEvent(struct inotify_event *event)
{
    try {
        std::map<std::string, std::unique_ptr<FontFolder>>::iterator it;
        for (it = m_fontMonitorFolders.begin(); it != m_fontMonitorFolders.end(); it++) {
            if (event->wd == it->second->GetWd()) {
                if (event->mask & (IN_CREATE|IN_CLOSE_WRITE|IN_MOVED_TO)) {
                    std::string fonts_dir(it->second->GetPath());
                    fonts_dir += "/";
                    fonts_dir += event->name;
                    THROW_LAST_ERROR_IF(access(fonts_dir.c_str(), R_OK) != 0);
                    it->second->ModifyX11FontPath(true);
                } else if (event->mask & (IN_DELETE|IN_MOVED_FROM)) {
                    it->second->ModifyX11FontPath(false);
                }
                break;
            }
        }
    }
    CATCH_LOG();
}
        
void* wslgd::FontMonitor::FontMonitorThread(void *context)
{
    FontMonitor *This = reinterpret_cast<FontMonitor*>(context);
    struct inotify_event *event;
    int len, cur;
    char buf[10 * (sizeof *event + 256)];

    LOG_INFO("FontMonitor: monitoring thread started.");

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    // Dump currently tracking folders.
    This->DumpMonitorFolders();

    // Start listening folder add/remove.
    for (;;) {
        len = read(This->GetFd(), buf, sizeof buf);
        cur = 0;
        while (cur < len) {
            event = (struct inotify_event *)&buf[cur];
            if (event->len) {
                if (event->mask & IN_ISDIR) {
                    // A directory is added or removed.
                    This->HandleFolderEvent(event);
                } else if (strcmp(event->name, c_fontsdir) == 0) {
                    // A fonts.dir is added or removed.
                    This->HandleFontsDirEvent(event);
                }
            }
            cur += (sizeof *event + event->len);
        }
    }
    // never hit here.
    assert(true);

    return 0;
}

int wslgd::FontMonitor::Start()
{
    bool succeeded = false;

    assert(m_fontMonitorFolders.empty());
    assert(!m_fontMonitorThread);

    try {
        // xset must be installed.
        THROW_LAST_ERROR_IF(access(c_xset, X_OK) < 0);

        // if user distro mount folder does not exist, bail out.
        THROW_LAST_ERROR_IF_FALSE(std::filesystem::exists(USER_DISTRO_MOUNT_PATH));
        // and check fonts path inside user distro.
        THROW_LAST_ERROR_IF_FALSE(std::filesystem::exists(USER_DISTRO_FONTPATH));

        // start monitoring on mounted font folder.
        wil::unique_fd fd(inotify_init());
        THROW_LAST_ERROR_IF(!fd);
        m_fd.reset(fd.release());
        AddMonitorFolder(USER_DISTRO_FONTPATH);

        // Create font folder monitor thread.
        THROW_LAST_ERROR_IF(pthread_create(&m_fontMonitorThread, NULL, FontMonitorThread, (void*)this) < 0);

        succeeded = true;
    }
    CATCH_LOG();

    if (!succeeded) {
        Stop();
        return -1;
    }

    return 0;
}

void wslgd::FontMonitor::Stop()
{
    // Stop font folder monitor thread.
    if (m_fontMonitorThread) {
        pthread_cancel(m_fontMonitorThread);
        pthread_join(m_fontMonitorThread, NULL);
        m_fontMonitorThread = 0;
    }

    RemoveMonitorFolder(USER_DISTRO_FONTPATH);
    m_fontMonitorFolders.clear();

    m_fd.reset();

    LOG_INFO("FontMonitor: monitoring stopped.");
}
