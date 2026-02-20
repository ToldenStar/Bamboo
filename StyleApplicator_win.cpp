// bamboo/platform/StyleApplicator_win.cpp
// Windows-specific WindowStyle application.
// Uses DWM APIs for Mica, Acrylic, transparency, and rounded corners.

#include "bamboo/platform/StyleApplicator.hpp"
#include "include/cef_browser.h"

#if defined(_WIN32)
#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

// Windows 11 DWM attribute constants (not in all SDK versions)
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_MICA_EFFECT
#define DWMWA_MICA_EFFECT 1029
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

// DWM_SYSTEMBACKDROP_TYPE values
#define DWMSBT_NONE       1
#define DWMSBT_MAINWINDOW 2  // Mica
#define DWMSBT_TRANSIENT  3  // Acrylic
#define DWMSBT_TABBEDWINDOW 4 // Mica Alt / Tabbed

// DWM_WINDOW_CORNER_PREFERENCE values
#define DWMWCP_DEFAULT    0
#define DWMWCP_DONOTROUND 1
#define DWMWCP_ROUND      2
#define DWMWCP_ROUNDSMALL 3

namespace bamboo::platform {

namespace {

HWND getHWND(CefRefPtr<CefBrowser> browser) {
    if (!browser) return nullptr;
    return browser->GetHost()->GetWindowHandle();
}

bool isWindows11() {
    OSVERSIONINFOEXW osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwBuildNumber = 22000;
    DWORDLONG mask = VerSetConditionMask(0, VER_BUILDNUMBER, VER_GREATER_EQUAL);
    return VerifyVersionInfoW(&osvi, VER_BUILDNUMBER, mask) != FALSE;
}

void setDWMAttribute(HWND hwnd, DWORD attr, DWORD value) {
    DwmSetWindowAttribute(hwnd, attr, &value, sizeof(value));
}

void extendFrameIntoClient(HWND hwnd) {
    MARGINS margins = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}

} // namespace

void applyStyle(CefRefPtr<CefBrowser> browser, const WindowStyle& style) {
    HWND hwnd = getHWND(browser);
    if (!hwnd) return;

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    LONG winStyle = GetWindowLong(hwnd, GWL_STYLE);

    // ── Chrome mode ───────────────────────────────────────────────────────────
    switch (style.chromeMode) {
        case ChromeMode::Full:
            // CEF chrome runtime handles this
            break;

        case ChromeMode::NativeTitlebar:
            winStyle |= WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
            break;

        case ChromeMode::Frameless:
            winStyle &= ~(WS_CAPTION | WS_THICKFRAME);
            winStyle |= WS_POPUP;
            if (style.resizable) winStyle |= WS_SIZEBOX;
            break;

        case ChromeMode::CustomTitlebar:
            // Keep system controls but remove caption drawing
            winStyle |= WS_CAPTION | WS_SYSMENU;
            winStyle &= ~WS_THICKFRAME;
            extendFrameIntoClient(hwnd);
            break;
    }

    SetWindowLong(hwnd, GWL_STYLE, winStyle);

    // ── Transparency / material ───────────────────────────────────────────────
    if (style.transparent || style.backgroundOpacity < 1.0f) {
        exStyle |= WS_EX_LAYERED;
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
        SetLayeredWindowAttributes(hwnd, 0,
            static_cast<BYTE>(style.backgroundOpacity * 255),
            LWA_ALPHA);
    }

    // ── Windows material (Mica / Acrylic) ─────────────────────────────────────
    setWindowsMaterial(browser, style.windowsMaterial);

    // ── Always on top ─────────────────────────────────────────────────────────
    SetWindowPos(hwnd,
        style.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

    // ── Corner radius ─────────────────────────────────────────────────────────
    if (style.cornerRadius > 0) {
        setCornerRadius(browser, style.cornerRadius);
    } else {
        setDWMAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, DWMWCP_DEFAULT);
    }

    // ── Shadow ───────────────────────────────────────────────────────────────
    setShadow(browser, style.shadow);

    // ── Resize ───────────────────────────────────────────────────────────────
    setResizable(browser, style.resizable);

    // ── Taskbar visibility ────────────────────────────────────────────────────
    if (style.skipTaskbar) {
        exStyle |= WS_EX_TOOLWINDOW;
        exStyle &= ~WS_EX_APPWINDOW;
    } else {
        exStyle &= ~WS_EX_TOOLWINDOW;
        exStyle |= WS_EX_APPWINDOW;
    }
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

    // Flush style changes
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

void setDragRegions(CefRefPtr<CefBrowser> browser,
                    const std::vector<DragRegion>&)
{
    // On Windows, CEF's WM_NCHITTEST is used for drag.
    // The regions are stored in Browser and returned from the drag handler.
    // Trigger a repaint so CEF refreshes hit testing.
    if (HWND hwnd = getHWND(browser))
        InvalidateRect(hwnd, nullptr, FALSE);
}

void setCornerRadius(CefRefPtr<CefBrowser> browser, int radius) {
    HWND hwnd = getHWND(browser);
    if (!hwnd || !isWindows11()) return;

    DWORD pref = (radius <= 0)  ? DWMWCP_DONOTROUND :
                 (radius <= 4)  ? DWMWCP_ROUNDSMALL  :
                                  DWMWCP_ROUND;
    setDWMAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, pref);
}

void setMacOSVibrancy(CefRefPtr<CefBrowser>, MacOSVibrancy) {
    // No-op on Windows
}

void setWindowsMaterial(CefRefPtr<CefBrowser> browser, WindowsMaterial m) {
    HWND hwnd = getHWND(browser);
    if (!hwnd) return;

    if (!isWindows11() && m == WindowsMaterial::Mica) {
        m = WindowsMaterial::Acrylic;  // Fallback
    }

    // Enable DWM composition
    BOOL enable = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &enable, sizeof(enable));

    DWORD backdropType = DWMSBT_NONE;
    switch (m) {
        case WindowsMaterial::Mica:    backdropType = DWMSBT_MAINWINDOW;   break;
        case WindowsMaterial::MicaAlt: backdropType = DWMSBT_MAINWINDOW;   break;
        case WindowsMaterial::Acrylic: backdropType = DWMSBT_TRANSIENT;    break;
        case WindowsMaterial::Tabbed:  backdropType = DWMSBT_TABBEDWINDOW; break;
        default:                       backdropType = DWMSBT_NONE;         break;
    }

    // Windows 11 22H2+ supports DWMWA_SYSTEMBACKDROP_TYPE
    if (DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                               &backdropType, sizeof(backdropType)) != S_OK) {
        // Fallback: legacy Mica via DWMWA_MICA_EFFECT
        BOOL micaOn = (m == WindowsMaterial::Mica || m == WindowsMaterial::MicaAlt);
        DwmSetWindowAttribute(hwnd, DWMWA_MICA_EFFECT, &micaOn, sizeof(micaOn));
    }

    if (backdropType != DWMSBT_NONE) {
        extendFrameIntoClient(hwnd);
    }
}

void setBackgroundColor(CefRefPtr<CefBrowser> browser, Color c) {
    HWND hwnd = getHWND(browser);
    if (!hwnd) return;
    HBRUSH brush = CreateSolidBrush(RGB(c.r, c.g, c.b));
    SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(brush));
    InvalidateRect(hwnd, nullptr, TRUE);
}

void setTransparent(CefRefPtr<CefBrowser> browser, bool transparent, float opacity) {
    HWND hwnd = getHWND(browser);
    if (!hwnd) return;
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (transparent || opacity < 1.0f) {
        exStyle |= WS_EX_LAYERED;
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
        SetLayeredWindowAttributes(hwnd, 0, static_cast<BYTE>(opacity * 255), LWA_ALPHA);
    } else {
        exStyle &= ~WS_EX_LAYERED;
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    }
}

void setMacOSTitlebarButtonPosition(CefRefPtr<CefBrowser>, const TitlebarButtonPosition&) {
    // No-op on Windows
}

void setShadow(CefRefPtr<CefBrowser> browser, const Shadow& shadow) {
    HWND hwnd = getHWND(browser);
    if (!hwnd) return;
    // DWM shadow
    DWMNCRENDERINGPOLICY policy = shadow.enabled ? DWMNCRP_ENABLED : DWMNCRP_DISABLED;
    DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));
    DwmExtendFrameIntoClientArea(hwnd, shadow.enabled
        ? &(MARGINS{0,0,0,1})
        : &(MARGINS{0,0,0,0}));
}

void setResizable(CefRefPtr<CefBrowser> browser, bool resizable) {
    HWND hwnd = getHWND(browser);
    if (!hwnd) return;
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    if (resizable) style |= (WS_SIZEBOX | WS_MAXIMIZEBOX);
    else           style &= ~(WS_SIZEBOX | WS_MAXIMIZEBOX);
    SetWindowLong(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, nullptr, 0,0,0,0,
        SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
}

} // namespace bamboo::platform
#endif // _WIN32
