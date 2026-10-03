#pragma once

// Portable stand-ins for the handful of Windows GDI/bitmap types and legacy
// global-memory calls that LoadBMP.cpp and OptionsSubMultiplayer.cpp need.
// The BMP structs are read directly from file bytes (fread-style), so their
// size and layout have to match the real Windows definitions exactly - hence
// the explicit types and the 2-byte packing on BITMAPFILEHEADER (Windows'
// own <wingdi.h> packs it the same way, since WORD+DWORD would otherwise be
// padded to a 4-byte boundary and the struct would come out 16 bytes instead
// of the 14 the BMP file format actually uses).
#ifndef _WIN32

#include <cstdint>
#include <cstdlib>
#include <cstring>

#ifndef DIB_HEADER_MARKER
#define DIB_HEADER_MARKER 0x4D42 // 'BM'
#endif

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef void* HANDLE;
typedef DWORD COLORREF;

#ifndef RGB
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)(BYTE)(g) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#endif

#pragma pack(push, 2)
typedef struct tagBITMAPFILEHEADER
{
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
} BITMAPFILEHEADER;
#pragma pack(pop)

typedef struct tagBITMAPINFOHEADER
{
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagRGBQUAD
{
    BYTE rgbBlue;
    BYTE rgbGreen;
    BYTE rgbRed;
    BYTE rgbReserved;
} RGBQUAD;

typedef struct tagBITMAPINFO
{
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[1];
} BITMAPINFO, *LPBITMAPINFO;

// The older (OS/2-style) BMP core header, still checked for by some of this
// legacy loading code alongside the newer BITMAPINFOHEADER above.
typedef struct tagRGBTRIPLE
{
    BYTE rgbtBlue;
    BYTE rgbtGreen;
    BYTE rgbtRed;
} RGBTRIPLE;

typedef struct tagBITMAPCOREHEADER
{
    DWORD bcSize;
    WORD bcWidth;
    WORD bcHeight;
    WORD bcPlanes;
    WORD bcBitCount;
} BITMAPCOREHEADER;

typedef struct tagBITMAPCOREINFO
{
    BITMAPCOREHEADER bmciHeader;
    RGBTRIPLE bmciColors[1];
} BITMAPCOREINFO, *LPBITMAPCOREINFO;

// GlobalAlloc/GlobalLock/GlobalFree are 16-bit-Windows-era memory APIs kept
// around for compatibility; on any modern OS they're just malloc/free (the
// "lock" step did something when memory could still be moved around, which
// hasn't been true for decades).
#define GMEM_MOVEABLE 0
#define GMEM_ZEROINIT 0
typedef void* HGLOBAL;

inline HGLOBAL GlobalAlloc(unsigned int flags, size_t size)
{
    void* p = malloc(size);
    if (p && (flags & GMEM_ZEROINIT))
        memset(p, 0, size);
    return p;
}

inline void* GlobalLock(HGLOBAL mem)
{
    return mem;
}

inline void GlobalUnlock(HGLOBAL)
{
}

inline void GlobalFree(HGLOBAL mem)
{
    free(mem);
}

typedef char* LPSTR;

#endif // !_WIN32
