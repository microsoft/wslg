// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once
#define SHARE_PATH "/mnt/wslg"
#define USER_DISTRO_MOUNT_PATH SHARE_PATH "/distro"

void LogPrint(int level, const char *func, int line, const char *fmt, ...) noexcept;
#define LOG_LEVEL_EXCEPTION 3
#define LOG_LEVEL_ERROR     4
#define LOG_LEVEL_INFO      5
#define LOG_ERROR(fmt, ...) LogPrint(LOG_LEVEL_ERROR, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) LogPrint(LOG_LEVEL_INFO, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
