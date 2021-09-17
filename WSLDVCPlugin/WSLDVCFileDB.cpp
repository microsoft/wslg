// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include "pch.h"
#include "utils.h"
#include "WSLDVCFileDB.h"

#include <string>
#include <set>

class fileEntry
{
public:

    wstring getFileId() const
    {
        return m_fileId;
    }
    wstring getLinkFilePath() const
    {
        return m_linkFilePath;
    }
    wstring getIconFilePath() const
    {
        return m_iconFilePath;
    }

    //fileEntry(const fileEntry& r)
    //{
    //    m_fileId = r.m_fileId;
    //    m_linkFilePath = r.m_linkFilePath;
    //    m_iconFilePath = r.m_iconFilePath;
    //}
    fileEntry(const wchar_t* fileId)
    {
        assert(fileId);
        m_fileId = fileId;
        m_linkFilePath = L"";
        m_iconFilePath = L"";
    }
    fileEntry(const wchar_t* fileId, const wchar_t* linkPath, const wchar_t* iconPath)
    {
        assert(fileId);
        m_fileId = fileId;
        m_linkFilePath = linkPath;
        m_iconFilePath = iconPath;
    }

    bool operator< (LPWSTR key) const
    {
        wstring r = key;
        return getFileId() < r;
    }
    bool operator< (const fileEntry& r) const
    {
        return getFileId() < r.getFileId();
    }

private:

    wstring m_fileId;
    wstring m_linkFilePath;
    wstring m_iconFilePath;
};

class WSLDVCFileDB :
    public RuntimeClass<
    RuntimeClassFlags<ClassicCom>,
    IWSLDVCFileDB>
{
public:

    HRESULT
        RuntimeClassInitialize(
            void* pContext
        )
    {
        UNREFERENCED_PARAMETER(pContext);
        DebugPrint(L"RuntimeClassInitialize(): WSLDVCFileDB iniitalized: %p\n", this);
        return S_OK;
    }

    STDMETHODIMP
        OnFileAdded(_In_z_ LPCWSTR key, _In_opt_z_ LPCWSTR linkFilePath, _In_opt_z_ LPCWSTR iconFilePath)
    {
        HRESULT hr = S_OK;
        set<fileEntry>::iterator it;
        std::pair<set<fileEntry>::iterator, bool> p;

        DebugPrint(L"OnFileAdded():\n");
        DebugPrint(L"\tkey: %s\n", key);
        if (linkFilePath && lstrlenW(linkFilePath))
        {
            DebugPrint(L"\tlinkFilePath: %s\n", linkFilePath);
        }
        else
        {
            linkFilePath = L"";
        }
        if (iconFilePath && lstrlenW(iconFilePath))
        {
            DebugPrint(L"\ticonFilePath: %s\n", iconFilePath);
        }
        else
        {
            iconFilePath = L"";
        }

        fileEntry fileEntry(key, linkFilePath, iconFilePath);
        p = m_fileEntries.insert(fileEntry);
        if (!p.second)
        {
            if (p.first->getLinkFilePath().compare(linkFilePath) != 0 ||
                p.first->getIconFilePath().compare(iconFilePath) != 0)
            {
                DebugPrint(L"\tKey: %s is already exists and has different path data\n", key);
                DebugPrint(L"\tlinkFilePath: %s\n", p.first->getLinkFilePath().c_str());
                DebugPrint(L"\ticonFilePath: %s\n", p.first->getIconFilePath().c_str());
                // TODO: implement update existing by erase and add.         
                // DebugAssert(false);
                return E_FAIL;
            }
        }

        return hr;
    }

    STDMETHODIMP
        OnFileRemoved(_In_z_ LPCWSTR key)
    {
        HRESULT hr = S_OK;
        set<fileEntry>::iterator it;

        DebugPrint(L"OnFileRemoved()\n");
        DebugPrint(L"\tkey: %s\n", key);

        it = m_fileEntries.find(key);
        if (it != m_fileEntries.end())
        {
            DebugPrint(L"\tKey found:\n");
            if (it->getLinkFilePath().length())
            {
                DebugPrint(L"\tlinkPath: %s\n", it->getLinkFilePath().c_str());
            }
            if (it->getIconFilePath().length())
            {
                DebugPrint(L"\ticonPath: %s\n", it->getIconFilePath().c_str());
            }
            m_fileEntries.erase(it);
        }
        else
        {
            DebugPrint(L"Key not found\n");
            hr = E_FAIL;
        }

        return hr;
    }

    STDMETHODIMP FindFiles(
        _In_z_ LPCWSTR key, 
        _Out_writes_z_(linkFilePathSize) LPWSTR linkFilePath, UINT32 linkFilePathSize,
        _Out_writes_z_(iconFilePathSize) LPWSTR iconFilePath, UINT32 iconFilePathSize)
    {
        set<fileEntry>::iterator it;

        assert(linkFilePath);
        assert(linkFilePathSize);
        assert(iconFilePath);
        assert(iconFilePathSize);

        *linkFilePath = NULL;
        *iconFilePath = NULL;

        DebugPrint(L"FindFiles()\n");
        DebugPrint(L"\tkey: %s\n", key);

        it = m_fileEntries.find(key);
        if (it != m_fileEntries.end())
        {
            DebugPrint(L"\tKey found:\n");
            DebugPrint(L"\tlinkPath: %s\n", it->getLinkFilePath().c_str());
            DebugPrint(L"\ticonPath: %s\n", it->getIconFilePath().c_str());
            if (wcscpy_s(linkFilePath, linkFilePathSize, it->getLinkFilePath().c_str()) != 0)
            {
                return E_FAIL;
            }
            if (wcscpy_s(iconFilePath, iconFilePathSize, it->getIconFilePath().c_str()) != 0)
            {
                return E_FAIL;
            }
            return S_OK;
        }
        else
        {
            DebugPrint(L"\tKey NOT found:\n");
            return E_FAIL;
        }
    }

    STDMETHODIMP 
        addAllFilesAsFileIdAt(_In_z_ LPCWSTR path)
    {
        //DebugPrint(L"Dump directory: %s\n", path);
        return addAllSubFolderFiles(path);
    }

    STDMETHODIMP
        deleteAllFileIdFiles()
    {
        set<fileEntry>::iterator it;

        DebugPrint(L"deleteAllFileIdFiles() - files to delete %d\n", m_fileEntries.size());

        for (it = m_fileEntries.begin(); it != m_fileEntries.end(); ) {
            DebugPrint(L"\tDelete %s\n", it->getFileId().c_str());
            if (!DeleteFileW(it->getFileId().c_str()))
            {
                DebugPrint(L"DeleteFile(%s) failed\n", it->getFileId().c_str());
            }
            else
            {
                wstring::size_type found = it->getFileId().find_last_of(L"\\");
                if (found != wstring::npos)
                {
                    wstring dir = it->getFileId().substr(0, found);
                    if (PathIsDirectoryEmptyW(dir.c_str()))
                    {
                        DebugPrint(L"%s is empty, removing\n", dir.c_str());
                        if (!RemoveDirectory(dir.c_str()))
                        {
                            DebugPrint(L"Failed to remove %s\n", dir.c_str());
                        }
                    }
                }
            }
            it = m_fileEntries.erase(it);
        }

        return S_OK;
    }

    STDMETHODIMP
        OnClose()
    {
        DebugPrint(L"OnClose(): WSLDVCFileDB closed: %p\n", this);

        m_fileEntries.clear();

        return S_OK;
    }

protected:

    HRESULT
        addAllSubFolderFiles(_In_z_ const wchar_t* path)
    {
        WIN32_FIND_DATA data = {};
        wstring s = path;
        s += L"\\*";

        HANDLE hFind = FindFirstFile(s.c_str(), &data);
        do {
            wstring sub = path;
            sub += L"\\";
            sub += data.cFileName;

            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                wstring file = data.cFileName;

                if (file == L"." || file == L"..")
                {
                    continue;
                }

                //DebugPrint(L"\tdirectory: %s\n", sub.c_str());
                if (PathIsDirectoryEmptyW(sub.c_str()))
                {
                    DebugPrint(L"%s is empty, removing\n", sub.c_str());
                    if (!RemoveDirectory(sub.c_str()))
                    {
                        DebugPrint(L"Failed to remove %s\n", sub.c_str());
                    }
                }
                else
                {
                    addAllSubFolderFiles(sub.c_str());
                }
            }
            else
            {
                //DebugPrint(L"\t\tfile: %s\n", sub.c_str());
                OnFileAdded(sub.c_str(), nullptr, nullptr);
            }
        } while (FindNextFile(hFind, &data));
        FindClose(hFind);

        return S_OK;
    }

    virtual
        ~WSLDVCFileDB()
    {
    }

private:
    set<fileEntry> m_fileEntries;
};

HRESULT
WSLDVCFileDB_CreateInstance(
    void* pContext,
    IWSLDVCFileDB** ppFileDB
)
{
    return MakeAndInitialize<WSLDVCFileDB>(ppFileDB, pContext);
}
