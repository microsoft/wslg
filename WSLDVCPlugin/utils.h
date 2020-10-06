#pragma once

//#ifdef _DEBUG
#define DBG_MESSAGE
//#endif // _DEBUG

#ifdef DBG_MESSAGE
void DebugPrint(const wchar_t* format, ...);
#define DebugAssert(exp) assert(exp)
#else
#define DebugPrint
#define DebugAssert
#endif // DBG_MESSAGE

HRESULT
CreateShellLink(_In_z_ LPCWSTR lpszPathLink,
    _In_z_ LPCWSTR lpszPathObj, 
    _In_z_ LPCWSTR lpszArgs,
    _In_z_ LPCWSTR lpszWorkingDir,
    _In_z_ LPCWSTR lpszDesc, 
	_In_z_ LPCWSTR lpszPathIcon);

HRESULT
CreateIconFile(BYTE* pBuffer,
    UINT32 cbSize,
    _In_z_ LPCWSTR lpszIconFile);

BOOL
GetLocaleName(_Out_writes_z_(localeNameSize) char* localeName,
    int localeNameSize);

#pragma pack(1)
//
// .ICO file format header
//

// Icon entry struct
typedef struct _ICON_DIR_ENTRY
{
    BYTE    bWidth;         // Width, in pixels, of the image
    BYTE    bHeight;        // Height, in pixels, of the image
    BYTE    bColorCount;    // Number of colors in image (0 if >=8bpp)
    BYTE    bReserved;      // Reserved ( must be 0)
    WORD    wPlanes;        // Color Planes
    WORD    wBitCount;      // Bits per pixel
    DWORD   dwBytesInRes;   // How many bytes in this resource?
    DWORD   dwImageOffset;  // Where in the file is this image?
} ICON_DIR_ENTRY;

// Icon directory struct
typedef struct _ICON_HEADER
{
    WORD           idReserved;   // Reserved (must be 0)
    WORD           idType;       // Resource Type (1 for icons)
    WORD           idCount;      // How many images?
    ICON_DIR_ENTRY idEntries[1]; // An entry for each image (idCount of 'em)
} ICON_HEADER;
#pragma pack()