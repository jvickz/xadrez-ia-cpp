#pragma once
#include <windows.h>

// The same multi-resolution logo is embedded in the executable and used by
// Explorer, the title bar, Alt+Tab and the running taskbar window.
inline HICON chessAppIcon(HINSTANCE instance, bool small) {
    return static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON,
        GetSystemMetrics(small ? SM_CXSMICON : SM_CXICON),
        GetSystemMetrics(small ? SM_CYSMICON : SM_CYICON), LR_SHARED));
}

inline void setChessWindowIcons(HWND window) {
    const auto instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(window, GWLP_HINSTANCE));
    SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(chessAppIcon(instance, true)));
    SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(chessAppIcon(instance, false)));
}
