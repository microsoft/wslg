// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include "FontMonitor.h"
#include "common.h"

constexpr auto c_fontMonitorRootPath = "/mnt/wslg/fonts/X11/";
constexpr auto c_fontsdir = "fonts.dir";
constexpr auto c_xset = "/usr/bin/xset";

wslgd::FontFolder::FontFolder(int fd, const char *path)
{
    LOG_INFO("FontMonitor: start monitoring %s", path);

    m_fd = fd;
    m_path = path;

    /* check if folder is already ready to be added to font path */
    std::string fonts_dir(path);
    fonts_dir += "/";
    fonts_dir += c_fontsdir;
    if (access(fonts_dir.c_str(), R_OK) == 0) {
        ModifyX11FontPath(true);
    } else {
        /* if the file does not exist nor accessible, add watch */
        LOG_INFO("FontMonitor: %s is not accessible, adding watch", fonts_dir.c_str());
        m_wd = inotify_add_watch(m_fd, path, IN_CREATE|IN_DELETE|IN_MOVED_TO|IN_MOVED_FROM);
        if (m_wd < 0) {
            /* if failed, nothing can be done, just log error */
            LOG_ERROR("FontMonitor: failed to add watch %s %s", path, strerror(errno));
        }
    }
}

wslgd::FontFolder::~FontFolder()
{
    LOG_INFO("FontMonitor: stop monitoring %s", m_path.c_str());

    if (m_isPathAdded) {
        ModifyX11FontPath(false);
    }

    /* if still under watch, remove it */
    if (m_wd >= 0) {
        inotify_rm_watch(m_fd, m_wd);
        m_wd = -1;
    }
}

void wslgd::FontFolder::ModifyX11FontPath(bool toAdd)
{
    std::string cmd(c_xset);
    if (toAdd)
       cmd += " +fp ";
    else
       cmd += " -fp ";
    cmd += m_path;
    LOG_INFO("FontMonitor: execuate %s", cmd.c_str());
    system(cmd.c_str());
    m_isPathAdded = toAdd;
    LOG_INFO("FontMonitor: %s is %s from X11 font path",
        m_path.c_str(), m_isPathAdded ? "added" : "removed");
}

wslgd::FontMonitor::FontMonitor()
{
}

void wslgd::FontMonitor::DumpMonitorFolders()
{
    std::map<std::string, std::unique_ptr<FontFolder>>::iterator it;
    for (it = m_fontMonitorFolders.begin(); it != m_fontMonitorFolders.end(); it++)
        LOG_INFO("FontMonitor: monitoring %s, and it is %s to X11 font path", it->first.c_str(),
            it->second->IsPathAdded() ? "added" : "*not* added");
}

void wslgd::FontMonitor::AddMonitorFolder(const char *path)
{
    std::string monitorPath(path);
    // checkf if path is tracked already.
    if (m_fontMonitorFolders.find(monitorPath) == m_fontMonitorFolders.end()) {
        std::unique_ptr<FontFolder> fontFolder(new FontFolder(m_fd, path));
        m_fontMonitorFolders.insert(std::make_pair(std::move(monitorPath), std::move(fontFolder)));
    } else {
        LOG_INFO("FontMonitor: %s is already tracked", path);
    }
}

void wslgd::FontMonitor::RemoveMonitorFolder(const char *path)
{
    std::string monitorPath(path);
    m_fontMonitorFolders.erase(monitorPath);
}

void wslgd::FontMonitor::HandleFolderEvent(struct inotify_event *event)
{
    if (event->wd == m_wd) {
        LOG_INFO("FontMonitor: font directroy change: %s/%s 0x%x",
               c_fontMonitorRootPath, event->name, event->mask);
        if (event->mask & (IN_CREATE|IN_MOVED_TO)) {
            std::string fullPath(c_fontMonitorRootPath);
            fullPath += event->name;
            AddMonitorFolder(fullPath.c_str());
        } else if (event->mask & (IN_DELETE|IN_MOVED_FROM)) {
            std::string fullPath(c_fontMonitorRootPath);
            fullPath += event->name;
            RemoveMonitorFolder(fullPath.c_str());
        }
    }
}

void wslgd::FontMonitor::HandleFontsDirEvent(struct inotify_event *event)
{
    std::map<std::string, std::unique_ptr<FontFolder>>::iterator it;
    for (it = m_fontMonitorFolders.begin(); it != m_fontMonitorFolders.end(); it++) {
        if (event->wd == it->second->GetWd()) {
            if (event->mask & (IN_CREATE|IN_MOVED_TO)) {
                std::string fonts_dir(it->second->GetPath());
                fonts_dir += "/";
                fonts_dir += event->name;
                if (access(fonts_dir.c_str(), R_OK) == 0) {
                    sleep(2); // TODO:
                    it->second->ModifyX11FontPath(true);
                } else {
                    LOG_ERROR("FontMonitor: %s is not accessible but reported by watch", fonts_dir.c_str());
                }
            } else if (event->mask & (IN_DELETE|IN_MOVED_FROM)) {
                it->second->ModifyX11FontPath(false);
            }
            break;
        }
    }
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
    assert(m_fd == -1);
    assert(m_wd == -1);

    try {
        // xset must be installed.
        THROW_LAST_ERROR_IF(access(c_xset, X_OK) < 0);

        // mount folder must be exists.
        THROW_LAST_ERROR_IF_FALSE(std::filesystem::exists(c_fontMonitorRootPath));

        // Start monitoring on root font folder.
        THROW_LAST_ERROR_IF((m_fd = inotify_init()) < 0);
        THROW_LAST_ERROR_IF((m_wd = inotify_add_watch(m_fd, c_fontMonitorRootPath, IN_CREATE|IN_DELETE|IN_MOVED_TO|IN_MOVED_FROM)) < 0);

        // Add existing folders to track.
        for (auto& dir_entry : std::filesystem::directory_iterator{c_fontMonitorRootPath}) {
            if (dir_entry.is_directory())
                AddMonitorFolder(dir_entry.path().c_str());
        }

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

    if (m_wd >= 0) {
        inotify_rm_watch(m_fd, m_wd);
        m_wd = -1;
    }

    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }

    m_fontMonitorFolders.clear();

    LOG_INFO("FontMonitor: monitoring stopped.");
}
