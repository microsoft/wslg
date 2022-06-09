// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include "pch.h"
#include "utils.h"
#include "WSLDVCPlugin.h"
#include "WSLDVCFileDB.h"
#include <cchannel.h>

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

    __declspec(dllexport) HRESULT WINAPI
        RemoveAppProvider(
            _In_z_ LPCWSTR appProvider
        )
    {
        HRESULT hr;

        WCHAR appMenuPath[MAX_PATH] = {};
        WCHAR iconPath[MAX_PATH] = {};

        ComPtr<IWSLDVCFileDB> spFileDB;

        if (!appProvider)
        {
            DebugPrint(L"appProvider parameter is required\n");
            return E_INVALIDARG;
        }

        hr = BuildMenuPath(ARRAYSIZE(appMenuPath), appMenuPath, appProvider, false);
        if (FAILED(hr))
        {
            return hr;
        }
        DebugPrint(L"AppMenuPath: %s\n", appMenuPath);

        if (!IsDirectoryPresent(appMenuPath))
        {
            DebugPrint(L"%s is not present\n", appMenuPath);
            return S_OK; // no program menu exists for given provider, simply exit.
        }

        hr = BuildIconPath(ARRAYSIZE(iconPath), iconPath, appProvider, false);
        if (FAILED(hr))
        {
            return hr;
        }
        DebugPrint(L"IconPath: %s\n", iconPath);

        hr = WSLDVCFileDB_CreateInstance(NULL, &spFileDB);
        if (FAILED(hr))
        {
            DebugPrint(L"failed to instance WSLDVCFileDB\n");
            return hr;
        }

        spFileDB->addAllFilesAsFileIdAt(appMenuPath);
        spFileDB->addAllFilesAsFileIdAt(iconPath);

        spFileDB->deleteAllFileIdFiles();
        spFileDB->OnClose();
        spFileDB = nullptr;

        return hr;
    }
}
