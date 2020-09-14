#pragma once

#include <tsvirtualchannels.h>

HRESULT
WSLDVCCallback_CreateInstance(
    IWTSVirtualChannel* pChannel,
    IWTSVirtualChannelCallback** ppCallback
);