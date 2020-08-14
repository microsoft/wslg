// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#define DEV_KMSG "/dev/kmsg"
#define ETH0 "eth0"
#define LOG_ERROR(str, ...) { fprintf(stderr, "<3>WSLGd: %s:%u: " str "\n",__FUNCTION__, __LINE__, ##__VA_ARGS__); }
#define LOG_INFO(str, ...) { fprintf(stderr, "<4>WSLGd: " str "\n", ##__VA_ARGS__); }
#define VSOCK_TEMPLATE "%08X-FACB-11E6-BD58-64006A7986D3"
#define SHARE_PATH "/mnt/wsl"
#define USERNAME "wslg"
#define X11_RUNTIME_DIR SHARE_PATH "/.X11-unix"
#define XDG_RUNTIME_DIR SHARE_PATH "/runtime-dir"

static int g_logFd = STDERR_FILENO;
