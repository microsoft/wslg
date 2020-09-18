#pragma once

//
// RDP APPLIST protocol header.
//
#define RDPAPPLIST_CMDID_CAPS 0x00000001
#define RDPAPPLIST_CMDID_UPDATE_APPLIST 0x00000002
#define RDPAPPLIST_CMDID_DELETE_APPLIST 0x00000003
#define RDPAPPLIST_CMDID_DELETE_APPLIST_PROVIDER 0x00000004

#define RDPAPPLIST_FIELD_ID       0x00000001
#define RDPAPPLIST_FIELD_GROUP    0x00000002
#define RDPAPPLIST_FIELD_EXECPATH 0x00000004
#define RDPAPPLIST_FIELD_DESC     0x00000008
#define RDPAPPLIST_FIELD_ICON     0x00000010
#define RDPAPPLIST_FIELD_PROVIDER 0x00000020

/* RDPAPPLIST_UPDATE_APPLIST_PDU */
#define RDPAPPLIST_HINT_NEWID      0x00010000 /* new appId vs update existing appId. */
#define RDPAPPLIST_HINT_SYNC       0x00100000 /* In sync mode (use with _NEWID). */
#define RDPAPPLIST_HINT_SYNC_START 0x00200000 /* Sync appId start (use with _SYNC). */
#define RDPAPPLIST_HINT_SYNC_END   0x00400000 /* Sync appId end (use with _SYNC). */

#define RDPAPPLIST_CHANNEL_VERSION 1

#define RDPAPPLIST_HEADER_SIZE 8

typedef struct _RDPAPPLIST_HEADER
{
    UINT32 cmdId;
    UINT32 length;
} RDPAPPLIST_HEADER;

typedef struct _RDPAPPLIST_CLIENT_CAPS_PDU
{
    UINT16 version;
} RDPAPPLIST_CLIENT_CAPS_PDU;

typedef struct _RDPAPPLIST_SERVER_CAPS_PDU
{
    UINT16 version;
    UINT16 appListProviderNameLength;
    WCHAR appListProviderName[64];
} RDPAPPLIST_SERVER_CAPS_PDU;

typedef struct _RDPAPPLIST_UPDATE_APPLIST_PDU
{
    UINT32 flags;
    UINT16 appIdLength;
    WCHAR appId[64];
    UINT16 appGroupLength;
    WCHAR appGroup[64];
    UINT16 appExecPathLength;
    WCHAR appExecPath[64];
    UINT16 appDescLength;
    WCHAR appDesc[64];
} RDPAPPLIST_UPDATE_APPLIST_PDU;

typedef struct _RDPAPPLIST_ICON_DATA
{
    UINT32 flags;
    UINT32 iconWidth;
    UINT32 iconHeight;
    UINT32 iconStride;
    UINT32 iconBpp;
    UINT32 iconFormat;
    UINT32 iconBitsLength;
    UINT32 iconFileSize;
    BYTE* iconFileData;
} RDPAPPLIST_ICON_DATA;

typedef struct _RDPAPPLIST_DELETE_APPLIST_PDU
{
    UINT32 flags;
    UINT16 appIdLength;
    WCHAR appId[64];
    UINT16 appGroupLength;
    WCHAR appGroup[64];
} RDPAPPLIST_DELETE_APPLIST_PDU;

typedef struct _RDPAPPLIST_DELETE_APPLIST_PROVIDER_PDU
{
    UINT32 flags;
    UINT16 appListProviderNameLength;
    WCHAR appListProviderName[64];
} RDPAPPLIST_DELETE_APPLIST_PROVIDER_PDU;

//
// Read macro.
//
#define LSTR(x) L ## x

// ReadUINT16(dest, source, remaining)
#define ReadUINT16(o, p, r)    \
    if (r >= sizeof(UINT16)) { \
        o = (*(UINT16*)(p));   \
        (p) += sizeof(UINT16); \
        (r) -= sizeof(UINT16); \
    } else {                   \
        DebugPrint(L"Failed to read " LSTR(#o) L"\n"); \
        goto Error_Read;       \
    }

// ReadUINT32(dest, source, remaining)
#define ReadUINT32(o, p, r)    \
    if (r >= sizeof(UINT32)) { \
        o = (*(UINT32*)(p));   \
        (p) += sizeof(UINT32); \
        (r) -= sizeof(UINT32); \
    } else {                   \
        DebugPrint(L"Failed to read " LSTR(#o) L"\n"); \
        goto Error_Read;       \
    }

// ReadBYTES(dest, source, lengthToCopy, RemainingSource)
#define ReadBYTES(o, p, l, r)  \
    if (r >= l) {              \
        memcpy((o), (p), (l)); \
        (p) += l;              \
        (r) -= (l);            \
    } else {                   \
        DebugPrint(L"Failed to read " LSTR(#o) L"\n"); \
        goto Error_Read;       \
    }

// ReadSTRING(dest, source, RemainingSource, required)
#define ReadSTRING(o, p, r, required) \
    ReadUINT16(o ## Length, p, r); \
    if (o ## Length > sizeof(o)) { \
        DebugPrint(L"Failed to read " LSTR(#o) L"\n"); \
        goto Error_Read; \
    } if (o ## Length) { \
        ReadBYTES(o, p, o ## Length, r); \
    } else if (required) { \
        DebugPrint(LSTR(#o) L" is required\n"); \
        goto Error_Read; \
    }