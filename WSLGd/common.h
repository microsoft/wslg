// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once
#define SHARE_PATH "/mnt/wslg"
#define USER_DISTRO_MOUNT_PATH SHARE_PATH "/distro"
#define LOG_ERROR(str, ...) { fprintf(stderr, "<3>WSLGd: %s:%u: " str "\n",__FUNCTION__, __LINE__, ##__VA_ARGS__); }
#define LOG_INFO(str, ...) { fprintf(stderr, "<4>WSLGd: " str "\n", ##__VA_ARGS__); }

