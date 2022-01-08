
#pragma once

// Target version information
// Modify the following defines if you have to target a platform prior to the ones pecified below.
// Refer to MSDN for the latest info on corresponding values for different platforms.

#ifndef WINVER              // Allow use of features pecific to Windows 7 or later.
#define WINVER 0x0701       // Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINNT        // Allow use of features pecific to Windows 7 or later.
#define _WIN32_WINNT 0x0701 // Change this to the appropriate value to target other versions of Windows.
#endif

// Include files.

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

// Windows Header Files:
#include <windows.h>

namespace DirectWriteGdiInterop {
    void init();
    bool hasDirectWrite();
    void draw_text(HDC hdc, int x, int y, wchar_t *text, int wn);
}
