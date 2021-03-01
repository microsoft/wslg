// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include "pch.h"
#include "WSLDVCListenerCallback.h"
#include "WSLDVCCallback.h"

class WSLDVCListenerCallback :
    public RuntimeClass<
    RuntimeClassFlags<ClassicCom>,
    IWTSListenerCallback>
{
public:

    HRESULT
        RuntimeClassInitialize()
    {
        return S_OK;
    }

    //
    // IWTSListenerCallback interface
    //
    STDMETHODIMP
        OnNewChannelConnection(
            __RPC__in_opt IWTSVirtualChannel* pChannel,
            __RPC__in_opt BSTR data,
            __RPC__out BOOL* pbAccept,
            __RPC__deref_out_opt IWTSVirtualChannelCallback** ppCallback
        )
    {
        UNREFERENCED_PARAMETER(data);

        HRESULT hr = WSLDVCCallback_CreateInstance(pChannel, ppCallback);
        if (SUCCEEDED(hr))
        {
            *pbAccept = TRUE;
        }

        return hr;
    }

protected:

    virtual
        ~WSLDVCListenerCallback()
    {

    }
};

HRESULT
WSLDVCListenerCallback_CreateInstance(
    IWTSListenerCallback** ppCallback
)
{
    return MakeAndInitialize<WSLDVCListenerCallback>(ppCallback);
}
