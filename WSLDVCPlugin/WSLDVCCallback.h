// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once

#include <tsvirtualchannels.h>

HRESULT
WSLDVCCallback_CreateInstance(
    IWTSVirtualChannel* pChannel,
    IWTSVirtualChannelCallback** ppCallback
);