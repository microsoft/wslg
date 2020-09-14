#include "pch.h"
#include "utils.h"
#include "rdpapplist.h"
#include "WSLDVCCallback.h"

LPCWSTR c_WSL_exe = L"%windir%\\system32\\wsl.exe";
LPCWSTR c_Working_dir = L"%windir%\\system32";

//
// This channel simply sends all the received messages back to the server. 
//
class WSLDVCCallback :
    public RuntimeClass<
    RuntimeClassFlags<ClassicCom>,
    IWTSVirtualChannelCallback>
{
public:
   
    HRESULT 
        RuntimeClassInitialize(
            IWTSVirtualChannel* pChannel
        )
    {
        m_spChannel = pChannel;
        return S_OK;
    }

    HRESULT
        ReadAppListHeader(
            UINT64 *size,
            BYTE** buffer,
            RDPAPPLIST_HEADER *appListHeader
        )
    {
        BYTE* cur;
        UINT64 len;

        assert(size);
        assert(buffer);
        assert(appListHeader);

        cur = *buffer;
        len = *size;

        ReadUINT32(appListHeader->cmdId, cur, len);
        ReadUINT32(appListHeader->length, cur, len);

        *buffer = cur;
        *size = len;

        return S_OK;

    Error_Read:

        return E_FAIL;
    }

    HRESULT
        ReadAppListServerCaps(
            UINT64 *size,
            BYTE** buffer,
            RDPAPPLIST_SERVER_CAPS_PDU *serverCaps
        )
    {
        BYTE* cur;
        UINT64 len;

        assert(size);
        assert(buffer);
        assert(serverCaps);

        cur = *buffer;
        len = *size;

        ReadUINT16(serverCaps->version, cur, len);
        if (m_serverCaps.version != RDPAPPLIST_CHANNEL_VERSION)
        {
            DebugPrint(L"Invalid applist version: %d", m_serverCaps.version);
            goto Error_Read;
        }
        ReadSTRING(serverCaps->appListProviderName, cur, len, true);
    
        *buffer = cur;
        *size = len;
        
        return S_OK;

    Error_Read:

        return E_FAIL;
    }

    HRESULT
        ReadAppListUpdate(
            UINT64* size,
            BYTE** buffer,
            RDPAPPLIST_UPDATE_APPLIST_PDU* updateAppList
        )
    {
        BYTE* cur;
        UINT64 len;

        assert(size);
        assert(buffer);
        assert(updateAppList);

        cur = *buffer;
        len = *size;

        ReadUINT32(updateAppList->flags, cur, len);
        ReadSTRING(updateAppList->appId, cur, len, true);
        ReadSTRING(updateAppList->appGroup, cur, len, false);
        ReadSTRING(updateAppList->appExecPath, cur, len, true);
        ReadSTRING(updateAppList->appDesc, cur, len, true);

        *buffer = cur;
        *size = len;

        return S_OK;

    Error_Read:

        return E_FAIL;
    }

    HRESULT
        ReadAppListIconData(
            UINT64* size,
            BYTE** buffer,
            RDPAPPLIST_ICON_DATA* iconData
        )
    {
        BYTE* cur;
        UINT64 len, fileSize;
        ICON_HEADER* pIconHeader;
        BITMAPINFOHEADER* pIconBitmapInfo;
        void* pIconBits;

        assert(size);
        assert(buffer);
        assert(iconData);

        cur = *buffer;
        len = *size;

        ZeroMemory(iconData, sizeof(*iconData));
        ReadUINT32(iconData->flags, cur, len);
        ReadUINT32(iconData->iconWidth, cur, len);
        ReadUINT32(iconData->iconHeight, cur, len);
        ReadUINT32(iconData->iconStride, cur, len);
        ReadUINT32(iconData->iconBpp, cur, len);
        ReadUINT32(iconData->iconFormat, cur, len);
        ReadUINT32(iconData->iconBitsLength, cur, len);

        iconData->iconFileSize = sizeof(ICON_HEADER) + sizeof(BITMAPINFOHEADER) + iconData->iconBitsLength;
        iconData->iconFileData = (BYTE*)LocalAlloc(LPTR, iconData->iconFileSize);
        if (!iconData->iconFileData)
        {
            return E_FAIL;
        }

        pIconHeader = (ICON_HEADER*)iconData->iconFileData;
        pIconBitmapInfo = (BITMAPINFOHEADER*)(pIconHeader + 1);
        pIconBits = (void*)(pIconBitmapInfo + 1);

        ReadBYTES(pIconBits, cur, iconData->iconBitsLength, len);

        pIconHeader->idReserved = 0; // Reserved (must be 0)
        pIconHeader->idType = 1;     // Resource Type (1 for icons)
        pIconHeader->idCount = 1;    // How many images?
        pIconHeader->idEntries[0].bWidth = (BYTE)iconData->iconWidth;   // Width, in pixels, of the image
        pIconHeader->idEntries[0].bHeight = (BYTE)iconData->iconHeight; // Height, in pixels, of the image
        pIconHeader->idEntries[0].bColorCount = 0;                      // Number of colors in image (0 if >=8bpp)
        pIconHeader->idEntries[0].bReserved = 0;                        // Reserved ( must be 0)
        pIconHeader->idEntries[0].wPlanes = 0;                          // Color Planes
        pIconHeader->idEntries[0].wBitCount = (WORD)iconData->iconBpp;  // Bits per pixel
        pIconHeader->idEntries[0].dwBytesInRes = sizeof(BITMAPINFOHEADER) + iconData->iconBitsLength; // How many bytes in this resource?
        pIconHeader->idEntries[0].dwImageOffset = sizeof(ICON_HEADER);  // Where in the file is this image?

        pIconBitmapInfo->biSize = sizeof(BITMAPINFOHEADER);
        pIconBitmapInfo->biWidth = iconData->iconWidth;
        pIconBitmapInfo->biHeight = iconData->iconHeight * 2;
        pIconBitmapInfo->biPlanes = 1;
        pIconBitmapInfo->biBitCount = iconData->iconBpp;
        pIconBitmapInfo->biCompression = BI_RGB;
        pIconBitmapInfo->biSizeImage = iconData->iconBitsLength;
        pIconBitmapInfo->biXPelsPerMeter = 0;
        pIconBitmapInfo->biYPelsPerMeter = 0;
        pIconBitmapInfo->biClrUsed = 0;
        pIconBitmapInfo->biClrImportant = 0;

#ifdef _DEBUG
        // Verify ICON.
        {
            HICON hIcon = CreateIconFromResource((BYTE*)pIconBitmapInfo, iconData->iconFileSize - sizeof(ICON_HEADER), TRUE, 0x00030000);
            if (!hIcon)
            {
                DebugPrint(L"CreateIconFromResource() failed\n");
            }
            else
            {
                DestroyIcon(hIcon);
            }
        }
#endif // _DEBUG

        *buffer = cur;
        *size = len;

        return S_OK;

    Error_Read:

        if (iconData->iconFileData)
        {
            LocalFree(iconData->iconFileData);
        }
        ZeroMemory(iconData, sizeof(*iconData));

        return E_FAIL;
    }
    
    HRESULT
        ReadAppListDelete(
            UINT64* size,
            BYTE** buffer,
            RDPAPPLIST_DELETE_APPLIST_PDU* deleteAppList
        )
    {
        BYTE* cur;
        UINT64 len;
        
        assert(size);
        assert(buffer);
        assert(deleteAppList);

        cur = *buffer;
        len = *size;

        ReadUINT32(deleteAppList->flags, cur, len);
        ReadSTRING(deleteAppList->appId, cur, len, true);
        ReadSTRING(deleteAppList->appGroup, cur, len, false);

        *buffer = cur;
        *size = len;

    Error_Read:

        return E_FAIL;
    }

    //
    // IWTSVirtualChannelCallback interface
    //

    STDMETHODIMP 
        OnDataReceived(
            ULONG cbSize,
            __RPC__in_ecount_full(cbSize) BYTE* pBuffer
        )
    {
        HRESULT hr = S_OK;
        BYTE* cur = pBuffer;
        UINT64 len = cbSize;

        DebugPrint(L"OnDataReceived enter, size = %d\n", len);

        while (len)
        {
            RDPAPPLIST_HEADER appListHeader = {};
            BYTE* header = cur;

            hr = ReadAppListHeader(&len, &cur, &appListHeader);
            if (FAILED(hr))
            {
                DebugPrint(L"Failed to read applist header\n");
                break;
            }

            if (appListHeader.cmdId == RDPAPPLIST_CMDID_CAPS)
            {
                hr = ReadAppListServerCaps(&len, &cur, &m_serverCaps);
                if (FAILED(hr))
                {
                    break;
                }

                PWSTR appMenuPath = NULL; // free by CoTaskMemFree. 
                SHGetKnownFolderPath(FOLDERID_StartMenu, 0, NULL, &appMenuPath);
                if (!appMenuPath)
                {
                    DebugPrint(L"SHGetKnownFolderPath(FOLDERID_StartMenu) failed\n");
                    hr = E_FAIL;
                    break;
                }

                memcpy(m_appGroup, m_serverCaps.appListProviderName, m_serverCaps.appListProviderNameLength);
                wsprintfW(m_appMenuPath, L"%s\\Programs\\%s", appMenuPath, m_appGroup);
                if (!CreateDirectoryW(m_appMenuPath, NULL))
                {
                    if (ERROR_ALREADY_EXISTS != GetLastError())
                    {
                        DebugPrint(L"Failed to create %s\n", m_appMenuPath);
                        hr = E_FAIL;
                        break;
                    }
                }
                CoTaskMemFree(appMenuPath);
                DebugPrint(L"AppMenuPath: %s\n", m_appMenuPath);

                UINT64 lenTempPath;
                lenTempPath = GetTempPathW(MAX_PATH, m_iconPath);
                if (!lenTempPath)
                {
                    DebugPrint(L"GetTempPathW failed\n");
                    hr = E_FAIL;
                    break;
                }

                if (lenTempPath + 5 +
                    (m_serverCaps.appListProviderNameLength / sizeof(WCHAR)) > ARRAYSIZE(m_iconPath))
                {
                    DebugPrint(L"provider name length check failed, length %d\n", m_serverCaps.appListProviderNameLength);
                     hr = E_FAIL;
                     break;
                }
 
                lstrcatW(m_iconPath, L"WSLDVCPlugin\\");
                if (!CreateDirectoryW(m_iconPath, NULL))
                {
                    if (ERROR_ALREADY_EXISTS != GetLastError())
                    {
                        DebugPrint(L"Failed to create %s\n", m_iconPath);
                        hr = E_FAIL;
                        break;
                    }
                }
                lstrcatW(m_iconPath, m_appGroup);
                if (!CreateDirectoryW(m_iconPath, NULL))
                {
                    if (ERROR_ALREADY_EXISTS != GetLastError())
                    {
                        DebugPrint(L"Failed to create %s\n", m_iconPath);
                        hr = E_FAIL;
                        break;
                    }
                }
                DebugPrint(L"IconPath: %s\n", m_iconPath);

                if (ExpandEnvironmentStringsW(c_WSL_exe, m_expandedPathObj, ARRAYSIZE(m_expandedPathObj)) == 0)
                {
                    DebugPrint(L"Failed to expand WSL exe: %s : %d\n", c_WSL_exe, GetLastError());
                    hr = E_FAIL;
                    break;
                }
                DebugPrint(L"WSL.exe: %s\n", m_expandedPathObj);

                if (ExpandEnvironmentStringsW(c_Working_dir, m_expandedWorkingDir, ARRAYSIZE(m_expandedWorkingDir)) == 0)
                {
                    DebugPrint(L"Failed to expand working dir: %s : %d\n", c_Working_dir, GetLastError());
                    hr = E_FAIL;
                    break;
                }
                DebugPrint(L"WSL.exe working dir: %s\n", m_expandedWorkingDir);

                // Eacho back header (8 bytes) + version (2 bytes) to server with length trimmed.
                union {
                    RDPAPPLIST_HEADER replyHeader;
                    BYTE replyBuf[10];
                };
                memcpy(replyBuf, header, 10);
                replyHeader.length = 10;
                hr = m_spChannel->Write(10, replyBuf, nullptr);
                if (FAILED(hr))
                {
                    DebugPrint(L"m_spChannel->Write failed, hr = %x\n", hr);
                    break;
                }
            }
            else if (appListHeader.cmdId == RDPAPPLIST_CMDID_UPDATE_APPLIST)
            {
                RDPAPPLIST_UPDATE_APPLIST_PDU updateAppList = {};
                RDPAPPLIST_ICON_DATA iconData = {};
                WCHAR linkPath[MAX_PATH] = {};
                WCHAR iconPath[MAX_PATH] = {};
                WCHAR exeArgs[MAX_PATH] = {};
                bool hasIcon = false;

                hr = ReadAppListUpdate(&len, &cur, &updateAppList);
                if (FAILED(hr))
                {
                    break;
                }

                if (updateAppList.flags & RDPAPPLIST_FIELD_ICON)
                {
                    hr = ReadAppListIconData(&len, &cur, &iconData);
                    if (FAILED(hr))
                    {
                        break;
                    }

                    lstrcpyW(iconPath, m_iconPath);
                    if (updateAppList.appGroupLength)
                    {
                        lstrcatW(iconPath, L"\\");
                        lstrcatW(iconPath, updateAppList.appGroup);
                        if (!CreateDirectoryW(iconPath, NULL))
                        {
                            if (ERROR_ALREADY_EXISTS != GetLastError())
                            {
                                DebugPrint(L"Failed to create %s\n", iconPath);
                                hr = E_FAIL;
                                break;
                            }
                        }
                    }
                    lstrcatW(iconPath, L"\\");
                    lstrcatW(iconPath, updateAppList.appId);
                    lstrcatW(iconPath, L".ico");
                    if (SUCCEEDED(CreateIconFile(iconData.iconFileData, iconData.iconFileSize, iconPath)))
                    {
                        hasIcon = true;
                    }
                    else
                    {
                        DebugPrint(L"Failed to create icon file %s\n", iconPath);
                        // Icon is optional, so keep going.
                    }
                }

                lstrcpyW(linkPath, m_appMenuPath);
                if (updateAppList.appGroupLength)
                {
                    lstrcatW(linkPath, L"\\");
                    lstrcatW(linkPath, updateAppList.appGroup);
                    if (!CreateDirectoryW(linkPath, NULL))
                    {
                        if (ERROR_ALREADY_EXISTS != GetLastError())
                        {
                            DebugPrint(L"Failed to create %s\n", linkPath);
                            hr = E_FAIL;
                            break;
                        }
                    }
                }
                lstrcatW(linkPath, L"\\");
                lstrcatW(linkPath, updateAppList.appId);
                lstrcatW(linkPath, L".lnk");

                wsprintfW(exeArgs,
                    L"~ -d %s %s",
                    m_appGroup, updateAppList.appExecPath);

                hr = CreateShellLink((LPCWSTR)linkPath,
                    (LPCWSTR)m_expandedPathObj,
                    (LPCWSTR)exeArgs,
                    (LPCWSTR)m_expandedWorkingDir,
                    (LPCWSTR)updateAppList.appDesc,
                    hasIcon ? (LPCWSTR)iconPath : NULL);
                if (FAILED(hr))
                {
                    break;
                }
            }
            else if (appListHeader.cmdId == RDPAPPLIST_CMDID_DELETE_APPLIST)
            {
                RDPAPPLIST_DELETE_APPLIST_PDU deleteAppList = {};
                WCHAR linkPath[MAX_PATH] = {};
                WCHAR iconPath[MAX_PATH] = {};

                hr = ReadAppListDelete(&len, &cur, &deleteAppList);
                if (FAILED(hr))
                {
                    break;
                }

                lstrcpyW(linkPath, m_appMenuPath);
                if (deleteAppList.appGroupLength)
                {
                    lstrcatW(linkPath, L"\\");
                    lstrcatW(linkPath, deleteAppList.appGroup);
                }
                lstrcatW(linkPath, L"\\");
                lstrcatW(linkPath, deleteAppList.appId);
                lstrcatW(linkPath, L".lnk");

                if (!DeleteFileW(linkPath))
                {
                    DebugPrint(L"DeleteFile(%s) failed, error %x\n", linkPath, GetLastError());
                }
                DebugPrint(L"Delete Path Link: %s\n", linkPath);

                lstrcpyW(iconPath, m_iconPath);
                if (deleteAppList.appGroupLength)
                {
                    lstrcatW(iconPath, L"\\");
                    lstrcatW(iconPath, deleteAppList.appGroup);
                }
                lstrcatW(iconPath, L"\\");
                lstrcatW(iconPath, deleteAppList.appId);
                lstrcatW(iconPath, L".ico");

                if (!DeleteFileW(iconPath))
                {
                    DebugPrint(L"DeleteFile(%s) failed, error %x\n", iconPath, GetLastError());
                }
                DebugPrint(L"Delete Icon Path: %s\n", iconPath);
            }
            else if (appListHeader.cmdId == RDPAPPLIST_CMDID_DELETE_APPLIST_PROVIDER)
            {
                // Nothing to do.
            }
            else
            {
                DebugPrint(L"Unknown command id:%d, Length %d\n", appListHeader.cmdId, appListHeader.length);
                hr = E_FAIL;
                break;
            }

            assert(len <= cbSize);
        }

        DebugPrint(L"OnDataReceived returns hr = %x\n", hr);
        return hr;
    }

    STDMETHODIMP 
        OnClose()
    {
        return S_OK;
    }

protected:

    virtual 
        ~WSLDVCCallback() 
    {
    }

private:
        
    ComPtr<IWTSVirtualChannel> m_spChannel;
    RDPAPPLIST_SERVER_CAPS_PDU m_serverCaps = {};
    WCHAR m_appGroup[MAX_PATH] = {};
    WCHAR m_appMenuPath[MAX_PATH] = {};
    WCHAR m_iconPath[MAX_PATH] = {};
    WCHAR m_expandedPathObj[MAX_PATH] = {};
    WCHAR m_expandedWorkingDir[MAX_PATH] = {};
};

HRESULT
    WSLDVCCallback_CreateInstance(
        IWTSVirtualChannel* pChannel,
        IWTSVirtualChannelCallback** ppCallback
    )
{
    return MakeAndInitialize<WSLDVCCallback>(ppCallback, pChannel);
}