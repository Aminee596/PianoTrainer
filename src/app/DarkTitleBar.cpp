#include "app/DarkTitleBar.h"

#if defined (_WIN32)
 #define WIN32_LEAN_AND_MEAN
 #define NOMINMAX
 #include <windows.h>
 #include <dwmapi.h>
 #pragma comment (lib, "dwmapi.lib")
#endif

namespace app
{
void useDarkTitleBar (void* handle, unsigned int rgb)
{
   #if defined (_WIN32)
    auto* hwnd = static_cast<HWND> (handle);
    if (hwnd == nullptr)
        return;

    const BOOL dark = TRUE;
    if (FAILED (DwmSetWindowAttribute (hwnd, 20, &dark, sizeof (dark))))
        DwmSetWindowAttribute (hwnd, 19, &dark, sizeof (dark));

    const COLORREF caption = RGB ((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
    const COLORREF text    = RGB (0xEC, 0xEE, 0xF3);
    DwmSetWindowAttribute (hwnd, 35 , &caption, sizeof (caption));
    DwmSetWindowAttribute (hwnd, 36 , &text, sizeof (text));
   #else
    (void) handle;
    (void) rgb;
   #endif
}

void bringToForeground (void* handle)
{
   #if defined (_WIN32)
    auto* hwnd = static_cast<HWND> (handle);
    if (hwnd == nullptr)
        return;
    if (IsIconic (hwnd))
        ShowWindow (hwnd, SW_RESTORE);
    const DWORD mine = GetCurrentThreadId();
    const HWND front = GetForegroundWindow();
    const DWORD theirs = front != nullptr ? GetWindowThreadProcessId (front, nullptr) : 0;
    const bool attach = theirs != 0 && theirs != mine;
    if (attach) AttachThreadInput (theirs, mine, TRUE);
    BringWindowToTop (hwnd);
    SetForegroundWindow (hwnd);
    SetFocus (hwnd);
    if (attach) AttachThreadInput (theirs, mine, FALSE);
   #else
    (void) handle;
   #endif
}

}
