// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include <sys/capability.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <assert.h>
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
#include <pthread.h>
#include <linux/vm_sockets.h>
#include <array>
#include <filesystem>
#include <map>
#include <new>
#include <vector>
#include "config.h"
#include "lxwil.h"
#if HAVE_WINPR
#include "winpr/ini.h"
#endif // HAVE_WINPR
