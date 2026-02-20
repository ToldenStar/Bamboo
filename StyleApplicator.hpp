// bamboo/platform/StyleApplicator.hpp
// Applies WindowStyle to the native OS window behind the CEF browser.
// Platform implementations are in StyleApplicator_win.cpp / _mac.mm / _linux.cpp
#pragma once

#include "bamboo/WindowStyle.hpp"
#include "include/cef_browser.h"

namespace bamboo::platform {

/**
 * @brief Apply a WindowStyle to the native window hosting the given CEF browser.
 *
 * Called on the CEF UI thread after the browser window is created and
 * whenever setStyle() is called at runtime.
 */
void applyStyle(CefRefPtr<CefBrowser> browser, const WindowStyle& style);

/**
 * @brief Set drag regions for a frameless window.
 */
void setDragRegions(CefRefPtr<CefBrowser> browser,
                    const std::vector<DragRegion>& regions);

/**
 * @brief Set corner radius (no-op on platforms that don't support it).
 */
void setCornerRadius(CefRefPtr<CefBrowser> browser, int radius);

/**
 * @brief Apply macOS vibrancy effect. No-op on non-macOS.
 */
void setMacOSVibrancy(CefRefPtr<CefBrowser> browser, MacOSVibrancy v);

/**
 * @brief Apply Windows Mica / Acrylic material. No-op on non-Windows.
 */
void setWindowsMaterial(CefRefPtr<CefBrowser> browser, WindowsMaterial m);

/**
 * @brief Set window background color (before HTML paints).
 */
void setBackgroundColor(CefRefPtr<CefBrowser> browser, Color c);

/**
 * @brief Make the window transparent (per-pixel alpha).
 */
void setTransparent(CefRefPtr<CefBrowser> browser, bool transparent, float opacity);

/**
 * @brief Position traffic-light buttons on macOS custom-titlebar windows.
 */
void setMacOSTitlebarButtonPosition(CefRefPtr<CefBrowser> browser,
                                    const TitlebarButtonPosition& pos);

/**
 * @brief Set window shadow.
 */
void setShadow(CefRefPtr<CefBrowser> browser, const Shadow& shadow);

/**
 * @brief Enable / disable the native window resize handle.
 */
void setResizable(CefRefPtr<CefBrowser> browser, bool resizable);

} // namespace bamboo::platform
