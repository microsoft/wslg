// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include "pch.h"
#include <cchannel.h>
#include "WSLDVCPlugin.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    UNREFERENCED_PARAMETER(hModule);
    UNREFERENCED_PARAMETER(lpReserved);

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

extern "C"
{
    __declspec(dllexport) HRESULT VCAPITYPE
        VirtualChannelGetInstance(
            _In_ REFIID refiid,
            _Inout_ ULONG* pNumObjs,
            _Out_opt_ VOID** ppObjArray
        )
    {
        HRESULT hr = S_OK;
        ComPtr<IWTSPlugin> spPlugin;

        if (refiid != __uuidof(IWTSPlugin))
        {
            return E_NOINTERFACE;
        }

        if (ppObjArray == NULL)
        {
            *pNumObjs = 1;
            return S_OK;
        }

        hr = WSLDVCPlugin_CreateInstance(&spPlugin);
        if (SUCCEEDED(hr))
        {
            *pNumObjs = 1;
            ppObjArray[0] = spPlugin.Detach();
        }

        return hr;
    }
}

