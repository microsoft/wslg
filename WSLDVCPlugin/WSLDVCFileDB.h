#pragma once

MIDL_INTERFACE("5802f934-1683-4e81-bb5a-7a0c29a2b1c7")
IWSLDVCFileDB : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE addAllFilesAsFileIdAt(wchar_t* path) = 0;
    virtual HRESULT STDMETHODCALLTYPE deleteAllFileIdFiles() = 0;

    virtual HRESULT STDMETHODCALLTYPE OnFileAdded(
            LPCWSTR key, LPCWSTR linkFilePath, LPCWSTR iconFilePath) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnFileRemoved(
            LPCWSTR key) = 0;

    virtual HRESULT STDMETHODCALLTYPE FindFiles(
            LPCWSTR key, 
            _Out_ LPWSTR linkFilePath, UINT32 linkFilePathSize,
            _Out_ LPWSTR iconFilePath, UINT32 iconFilePathSize) = 0;

    virtual HRESULT STDMETHODCALLTYPE OnClose(void) = 0;
};

HRESULT
WSLDVCFileDB_CreateInstance(
    void* pContext,
    IWSLDVCFileDB** ppFileDB
);