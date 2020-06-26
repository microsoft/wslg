// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once

MIDL_INTERFACE("5802f934-1683-4e81-bb5a-7a0c29a2b1c7")
IWSLDVCFileDB : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE addAllFilesAsFileIdAt(
        _In_z_ LPCWSTR path) = 0;
    virtual HRESULT STDMETHODCALLTYPE deleteAllFileIdFiles() = 0;

    virtual HRESULT STDMETHODCALLTYPE OnFileAdded(
        _In_z_ LPCWSTR key, 
        _In_opt_z_ LPCWSTR linkFilePath, 
        _In_opt_z_ LPCWSTR iconFilePath) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnFileRemoved(
        _In_z_ LPCWSTR key) = 0;

    virtual HRESULT STDMETHODCALLTYPE FindFiles(
        _In_z_ LPCWSTR key,
        _Out_writes_z_(linkFilePathSize) LPWSTR linkFilePath, UINT32 linkFilePathSize,
        _Out_writes_z_(iconFilePathSize) LPWSTR iconFilePath, UINT32 iconFilePathSize) = 0;

    virtual HRESULT STDMETHODCALLTYPE OnClose(void) = 0;
};

HRESULT
WSLDVCFileDB_CreateInstance(
    void* pContext,
    IWSLDVCFileDB** ppFileDB
);