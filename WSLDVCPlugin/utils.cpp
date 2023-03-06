// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include "pch.h"
#include "utils.h"

#ifdef DBG_MESSAGE
void DebugPrint(const wchar_t* format, ...)
{
    WCHAR buf[512] = {};
    va_list args;

    va_start(args, format);
    wvsprintfW(buf, format, args);
    va_end(args);

    OutputDebugStringW(buf);
}
#endif // DBG_MESSAGE

_Use_decl_annotations_
BOOL IsDirectoryPresent(LPCWSTR lpszPath)
{
    DWORD dwAttrib = GetFileAttributes(lpszPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
           (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

_Use_decl_annotations_
HRESULT
CreateShellLink(LPCWSTR lpszPathLink,
    LPCWSTR lpszPathObj,
    LPCWSTR lpszArgs,
    LPCWSTR lpszWorkingDir,
    LPCWSTR lpszDesc,
    LPCWSTR lpszPathIcon)
{
    HRESULT hr;
    IShellLink* psl;

    DebugPrint(L"CreateShellLink:\n");
    DebugPrint(L"\tPath Link: %s\n", lpszPathLink);
    DebugPrint(L"\tPath Exe: %s\n", lpszPathObj);
    DebugPrint(L"\tExe args: %s\n", lpszArgs);
    DebugPrint(L"\tWorkingDir: %s\n", lpszWorkingDir);
    DebugPrint(L"\tDesc: %s\n", lpszDesc);
    if (lpszPathIcon && lstrlenW(lpszPathIcon))
    {
        DebugPrint(L"\tIcon Path: %s\n", lpszPathIcon);
    }
    else
    {
        lpszPathIcon = nullptr;
    }

    // Get a pointer to the IShellLink interface. It is assumed that CoInitialize
    // has already been called.
    hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
    if (SUCCEEDED(hr))
    {
        IPersistFile* ppf;

        // Set the path to the shortcut target and add the description. 
        psl->SetPath(lpszPathObj);
        psl->SetArguments(lpszArgs);
        if (lpszPathIcon)
        {
            psl->SetIconLocation(lpszPathIcon, 0);
        }
        psl->SetDescription(lpszDesc);
        psl->SetWorkingDirectory(lpszWorkingDir);
        psl->SetShowCmd(SW_SHOWMINNOACTIVE);

        // Query IShellLink for the IPersistFile interface, used for saving the 
        // shortcut in persistent storage. 
        hr = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);
        if (SUCCEEDED(hr))
        {
            // Save the link by calling IPersistFile::Save. 
            hr = ppf->Save(lpszPathLink, TRUE);
            ppf->Release();
        }
        psl->Release();
    }

    DebugPrint(L"\tresult: %x\n", hr);
    return hr;
}

_Use_decl_annotations_
HRESULT
GetIconFileFromShellLink(
    UINT32 iconPathSize, 
    LPWSTR iconPath,
    LPCWSTR lnkPath)
{
    HRESULT hr;
    IShellLink* psl;

    DebugPrint(L"GetIconFileFromShellLink:\n");
    DebugPrint(L"\tPath Link: %s\n", lnkPath);

    assert(iconPathSize);
    assert(iconPath);
    *iconPath = L'\0';

    hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
    if (SUCCEEDED(hr))
    {
        IPersistFile* ppf;
        hr = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);
        if (SUCCEEDED(hr))
        {
            hr = ppf->Load(lnkPath, STGM_READ);
            if (SUCCEEDED(hr))
            {
                int dummy = 0;
                hr = psl->GetIconLocation(iconPath, iconPathSize, &dummy);
            }
            ppf->Release();
        }
        psl->Release();
    }

    DebugPrint(L"\tresult: %x\n", hr);
    if (SUCCEEDED(hr))
    {
        DebugPrint(L"\ticonPath: %s\n", iconPath);
    }

    return hr;
}

_Use_decl_annotations_
HRESULT
CreateIconFile(BYTE* pBuffer,
    UINT32 cbSize,
    LPCWSTR lpszIconFile)
{
    HRESULT hr = S_OK;
    HANDLE hFile;

    DebugPrint(L"CreateIconFile: %s\n", lpszIconFile);

    hFile = CreateFileW(lpszIconFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        DebugPrint(L"CreateFile(%s) failed, error %d\n", lpszIconFile, GetLastError());
        hr = E_FAIL;
    }
    else
    {        
        if (!WriteFile(hFile, pBuffer, cbSize, NULL, NULL))
        {
            DebugPrint(L"WriteFile(%s) failed, error %d\n", lpszIconFile, GetLastError());
            hr = E_FAIL;
        }
        
        CloseHandle(hFile);
    }

    DebugPrint(L"\tresult: %x\n", hr);
    return hr;
}

#define MAX_LOCALE_CODE 9

_Use_decl_annotations_
BOOL GetLocaleName(char* localeName, int localeNameSize)
{
    char langCode[MAX_LOCALE_CODE] = {};
    char countryName[MAX_LOCALE_CODE] = {};
    int result = 0;

    assert(localeName);
    localeName[0] = '\0';

    LCID lcid = MAKELCID(GetUserDefaultUILanguage(), SORT_DEFAULT);
    result = GetLocaleInfoA(lcid,
        LOCALE_SISO639LANGNAME,
        langCode,
        MAX_LOCALE_CODE) != 0;
    if ((result == 0) ||
        (strcpy_s(localeName, localeNameSize, langCode) != 0) ||
        (strcat_s(localeName, localeNameSize, "_") != 0))
    {
        return FALSE;
    }

    result = GetLocaleInfoA(lcid,
        LOCALE_SISO3166CTRYNAME,
        countryName,
        MAX_LOCALE_CODE) != 0;
    if ((result == 0) ||
        (strcat_s(localeName, localeNameSize, countryName) != 0))
    {
        return FALSE;
    }

    return TRUE;
}

_Use_decl_annotations_
HRESULT BuildMenuPath(
    UINT32 appMenuPathSize,
    LPWSTR appMenuPath,
    LPCWSTR appProvider,
    bool isCreateDir)
{
    PWSTR knownFolderPath = NULL; // free by CoTaskMemFree. 
    SHGetKnownFolderPath(FOLDERID_StartMenu, 0, NULL, &knownFolderPath);
    if (!knownFolderPath)
    {
        DebugPrint(L"SHGetKnownFolderPath(FOLDERID_StartMenu) failed\n");
        return E_FAIL;
    }
    int ret = swprintf_s(appMenuPath, appMenuPathSize, L"%s\\Programs\\%s", knownFolderPath, appProvider);
    CoTaskMemFree(knownFolderPath);
    if (ret < 0)
    {
        DebugPrint(L"swprintf_s for appMenuPath failed");
        return E_FAIL;
    }
    if (isCreateDir)
    {
        if (!CreateDirectoryW(appMenuPath, NULL))
        {
            if (ERROR_ALREADY_EXISTS != GetLastError())
            {
                DebugPrint(L"Failed to create %s\n", appMenuPath);
                return E_FAIL;
            }
        }
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT BuildIconPath(
    UINT32 iconPathSize,
    LPWSTR iconPath,
    LPCWSTR appProvider,
    bool isCreateDir)
{
    WCHAR prefix[] = L"WSLDVCPlugin\\";

    UINT32 lenTempPath;
    lenTempPath = GetTempPathW(iconPathSize, iconPath);
    if (!lenTempPath)
    {
        DebugPrint(L"GetTempPathW failed\n");
        return E_FAIL;
    }

    if ((lenTempPath + ARRAYSIZE(prefix) + wcslen(appProvider)) > iconPathSize)
    {
        DebugPrint(L"provider name length check failed, length %d\n",  wcslen(appProvider));
        return E_FAIL;
    }

    if (wcscat_s(iconPath, iconPathSize, prefix) != 0)
    {
        return E_FAIL;
    }
    if (isCreateDir)
    {
        if (!CreateDirectoryW(iconPath, NULL))
        {
            if (ERROR_ALREADY_EXISTS != GetLastError())
            {
                DebugPrint(L"Failed to create %s\n", iconPath);
                return E_FAIL;
            }
        }
    }
    if (wcscat_s(iconPath, iconPathSize, appProvider) != 0)
    {
        return E_FAIL;
    }
    if (isCreateDir)
    {
        if (!CreateDirectoryW(iconPath, NULL))
        {
            if (ERROR_ALREADY_EXISTS != GetLastError())
            {
                DebugPrint(L"Failed to create %s\n", iconPath);
                return E_FAIL;
            }
        }
    }

    return S_OK;
}

HRESULT
UpdateTaskBarInfo(
    HWND hwnd,
    _In_z_ LPCWSTR relaunchCmdline,
    _In_z_ LPCWSTR displayName,
    _In_z_ LPCWSTR iconPath)
{
    HRESULT hr;
    PROPVARIANT propvar;

    DebugPrint(L"UpdateTaskBarInfo: 0x%p\n", hwnd);
    DebugPrint(L"    relaunchCmdline: %s\n", relaunchCmdline);
    DebugPrint(L"    displayName: %s\n", displayName);
    DebugPrint(L"    iconPath: %s\n", iconPath);

    IPropertyStore* ps = NULL;

    hr = SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&ps));
    if (FAILED(hr))
    {
        DebugPrint(L"SHGetPropertyStoreForWindow failed: 0x%x\n", hr);
        return hr;
    }

    BOOL bPinToTaskbar = relaunchCmdline && displayName && iconPath;
    if (bPinToTaskbar)
    {
        hr = InitPropVariantFromString(displayName, &propvar);
        if (FAILED(hr))
        {
            DebugPrint(L"InitPropVariantFromString failed: 0x%x\n", hr);
            return hr;
        }

        hr = ps->SetValue(PKEY_AppUserModel_RelaunchDisplayNameResource, propvar);
        PropVariantClear(&propvar);
        if (FAILED(hr))
        {
            DebugPrint(L"SetValue(PKEY_AppUserModel_RelaunchDisplayNameResource failed: 0x%x\n", hr);
            return hr;
        }

        hr = InitPropVariantFromString(iconPath, &propvar);
        if (FAILED(hr))
        {
            DebugPrint(L"InitPropVariantFromString failed: 0x%x\n", hr);
            return hr;
        }

        hr = ps->SetValue(PKEY_AppUserModel_RelaunchIconResource, propvar);
        PropVariantClear(&propvar);
        if (FAILED(hr))
        {
            DebugPrint(L"SetValue(PKEY_AppUserModel_RelaunchIconResource failed: 0x%x\n", hr);
            return hr;
        }

        hr = InitPropVariantFromString(relaunchCmdline, &propvar);
        if (FAILED(hr))
        {
            DebugPrint(L"InitPropVariantFromString failed: 0x%x\n", hr);
            return hr;
        }

        hr = ps->SetValue(PKEY_AppUserModel_RelaunchCommand, propvar);
        PropVariantClear(&propvar);
        if (FAILED(hr))
        {
            DebugPrint(L"SetValue(PKEY_AppUserModel_RelaunchCommand failed: 0x%x\n", hr);
            return hr;
        }
    }
    else
    {
        hr = InitPropVariantFromBoolean(TRUE, &propvar);
        if (FAILED(hr))
        {
            DebugPrint(L"InitPropVariantFromBoolean failed: 0x%x\n", hr);
            return hr;
        }

        hr = ps->SetValue(PKEY_AppUserModel_PreventPinning, propvar);
        PropVariantClear(&propvar);
        if (FAILED(hr))
        {
            DebugPrint(L"SetValue(PKEY_AppUserModel_PreventPinning failed: 0x%x\n", hr);
            return hr;
        }
    }

    ps->Release();

    return S_OK;
}

#if ENABLE_WSL_SIGNATURE_CHECK
// Link with the Wintrust.lib file.
#pragma comment (lib, "wintrust")

#include <Softpub.h>
#include <wincrypt.h>
#include <wintrust.h>

_Use_decl_annotations_
BOOL IsFileTrusted(LPCWSTR pwszFile)
{
    BOOL bTrusted = FALSE;

    LONG lStatus;
    DWORD dwLastError;

    // Initialize the WINTRUST_FILE_INFO structure.
    WINTRUST_FILE_INFO FileData;
    memset(&FileData, 0, sizeof(FileData));
    FileData.cbStruct = sizeof(WINTRUST_FILE_INFO);
    FileData.pcwszFilePath = pwszFile;
    FileData.hFile = NULL;
    FileData.pgKnownSubject = NULL;

    /*
    WVTPolicyGUID specifies the policy to apply on the file
    WINTRUST_ACTION_GENERIC_VERIFY_V2 policy checks:
    
    1) The certificate used to sign the file chains up to a root 
    certificate located in the trusted root certificate store. This 
    implies that the identity of the publisher has been verified by 
    a certification authority.
    
    2) In cases where user interface is displayed (which this example
    does not do), WinVerifyTrust will check for whether the  
    end entity certificate is stored in the trusted publisher store,  
    implying that the user trusts content from this publisher.
    
    3) The end entity certificate has sufficient permission to sign 
    code, as indicated by the presence of a code signing EKU or no 
    EKU.
    */

    GUID WVTPolicyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_DATA WinTrustData;

    // Initialize the WinVerifyTrust input data structure.

    // Default all fields to 0.
    memset(&WinTrustData, 0, sizeof(WinTrustData));

    WinTrustData.cbStruct = sizeof(WinTrustData);
    
    // Use default code signing EKU.
    WinTrustData.pPolicyCallbackData = NULL;

    // No data to pass to SIP.
    WinTrustData.pSIPClientData = NULL;

    // Disable WVT UI.
    WinTrustData.dwUIChoice = WTD_UI_NONE;

    // No revocation checking.
    WinTrustData.fdwRevocationChecks = WTD_REVOKE_NONE; 

    // Verify an embedded signature on a file.
    WinTrustData.dwUnionChoice = WTD_CHOICE_FILE;

    // Verify action.
    WinTrustData.dwStateAction = WTD_STATEACTION_VERIFY;

    // Verification sets this value.
    WinTrustData.hWVTStateData = NULL;

    // Not used.
    WinTrustData.pwszURLReference = NULL;

    // This is not applicable if there is no UI because it changes 
    // the UI to accommodate running applications instead of 
    // installing applications.
    WinTrustData.dwUIContext = 0;

    // Set pFile.
    WinTrustData.pFile = &FileData;

    // WinVerifyTrust verifies signatures as specified by the GUID 
    // and Wintrust_Data.
    lStatus = WinVerifyTrust(
        NULL,
        &WVTPolicyGUID,
        &WinTrustData);

    switch (lStatus) 
    {
        case ERROR_SUCCESS:
            //Signed file:
            //    - Hash that represents the subject is trusted.
            //    - Trusted publisher without any verification errors.
            DebugPrint(L"The file \"%s\" is signed and the signature was verified.\n",
                pwszFile);
            bTrusted = TRUE;
            break;
        
        case TRUST_E_NOSIGNATURE:
            // The file was not signed or had a signature 
            // that was not valid.
            // Get the reason for no signature.
            dwLastError = GetLastError();
            if (TRUST_E_NOSIGNATURE == dwLastError ||
                    TRUST_E_SUBJECT_FORM_UNKNOWN == dwLastError ||
                    TRUST_E_PROVIDER_UNKNOWN == dwLastError) 
            {
                // The file was not signed.
                DebugPrint(L"The file \"%s\" is not signed.\n",
                    pwszFile);
            } 
            else 
            {
                // The signature was not valid or there was an error 
                // opening the file.
                DebugPrint(L"An unknown error occurred trying to "
                    L"verify the signature of the \"%s\" file.\n",
                    pwszFile);
            }
            break;

        case TRUST_E_EXPLICIT_DISTRUST:
            // The hash that represents the subject or the publisher 
            // is not allowed by the admin or user.
            DebugPrint(L"The signature is present, but specifically disallowed.\n");
            break;

        case TRUST_E_SUBJECT_NOT_TRUSTED:
            DebugPrint(L"The signature is present, but not trusted.\n");
            break;

        default:
            // The UI was disabled in dwUIChoice or the admin policy 
            // has disabled user trust. lStatus contains the 
            // publisher or time stamp chain error.
            DebugPrint(L"Error is: 0x%x.\n", lStatus);
            break;
    }

    // Any hWVTStateData must be released by a call with close.
    WinTrustData.dwStateAction = WTD_STATEACTION_CLOSE;

    lStatus = WinVerifyTrust(
        NULL,
        &WVTPolicyGUID,
        &WinTrustData);

    return bTrusted;
}
#endif // ENABLE_WSL_SIGNATURE_CHECK
