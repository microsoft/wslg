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
    DebugPrint(L"\tIcon Path: %s\n", lpszPathIcon);

    // Get a pointer to the IShellLink interface. It is assumed that CoInitialize
    // has already been called.
    hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
    if (SUCCEEDED(hr))
    {
        IPersistFile* ppf;

        // Set the path to the shortcut target and add the description. 
        psl->SetPath(lpszPathObj);
        psl->SetArguments(lpszArgs);
        psl->SetIconLocation(lpszPathIcon, 0);
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

HRESULT
CreateIconFile(BYTE* pBuffer,
    UINT32 cbSize,
    WCHAR* lpszIconFile)
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
