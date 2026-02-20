// bamboo/platform/StyleApplicator_mac.mm
// macOS-specific WindowStyle application using AppKit.
// Compiled as Objective-C++ (.mm) so we can mix Cocoa APIs with C++.

#include "bamboo/platform/StyleApplicator.hpp"
#include "include/cef_browser.h"

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

namespace bamboo::platform {

namespace {

// Retrieve the NSWindow from a CEF browser.
NSWindow* getNSWindow(CefRefPtr<CefBrowser> browser) {
    if (!browser) return nil;
    CefWindowHandle handle = browser->GetHost()->GetWindowHandle();
    // On macOS, GetWindowHandle() returns the NSView*.
    NSView* view = (__bridge NSView*)handle;
    return view ? view.window : nil;
}

NSVisualEffectMaterial toNSMaterial(MacOSVibrancy v) {
    switch (v) {
        case MacOSVibrancy::Sidebar:               return NSVisualEffectMaterialSidebar;
        case MacOSVibrancy::Menu:                  return NSVisualEffectMaterialMenu;
        case MacOSVibrancy::Popover:               return NSVisualEffectMaterialPopover;
        case MacOSVibrancy::HudWindow:             return NSVisualEffectMaterialHUDWindow;
        case MacOSVibrancy::UnderWindowBackground: return NSVisualEffectMaterialUnderWindowBackground;
        case MacOSVibrancy::UnderPageBackground:   return NSVisualEffectMaterialUnderPageBackground;
        case MacOSVibrancy::Titlebar:              return NSVisualEffectMaterialTitlebar;
        case MacOSVibrancy::HeaderView:            return NSVisualEffectMaterialHeaderView;
        case MacOSVibrancy::Sheet:                 return NSVisualEffectMaterialSheet;
        case MacOSVibrancy::WindowBackground:      return NSVisualEffectMaterialWindowBackground;
        case MacOSVibrancy::ContentBackground:     return NSVisualEffectMaterialContentBackground;
        case MacOSVibrancy::FullScreenUI:          return NSVisualEffectMaterialFullScreenUI;
        default:                                   return NSVisualEffectMaterialWindowBackground;
    }
}

// Remove any existing NSVisualEffectView from the window's content view.
void removeVibrancy(NSWindow* window) {
    if (!window) return;
    NSView* content = window.contentView;
    for (NSView* sub in [content.subviews reverseObjectEnumerator]) {
        if ([sub isKindOfClass:[NSVisualEffectView class]]) {
            [sub removeFromSuperview];
        }
    }
}

} // namespace

void applyStyle(CefRefPtr<CefBrowser> browser, const WindowStyle& style) {
    NSWindow* win = getNSWindow(browser);
    if (!win) return;

    // ── Chrome mode ───────────────────────────────────────────────────────────
    switch (style.chromeMode) {
        case ChromeMode::Full:
            // Nothing to strip — CEF's full Chrome UI is the default in CefSettings.chrome_runtime
            break;

        case ChromeMode::NativeTitlebar:
            // Standard macOS titlebar, no custom chrome
            win.titlebarAppearsTransparent = NO;
            win.titleVisibility = NSWindowTitleVisible;
            win.styleMask |= NSWindowStyleMaskTitled;
            break;

        case ChromeMode::Frameless:
            win.styleMask = NSWindowStyleMaskBorderless
                          | NSWindowStyleMaskResizable
                          | (style.resizable ? NSWindowStyleMaskResizable : 0);
            [win setMovableByWindowBackground:NO];
            break;

        case ChromeMode::CustomTitlebar:
            if (style.titlebar.macosHidden) {
                // "Hidden titlebar" — traffic lights float over content
                win.titlebarAppearsTransparent = YES;
                win.titleVisibility = NSWindowTitleHidden;
                win.styleMask |= NSWindowStyleMaskFullSizeContentView;
            } else {
                win.titlebarAppearsTransparent = style.titlebar.transparentWhenInactive;
            }
            break;
    }

    // ── Traffic light position ────────────────────────────────────────────────
    if (style.titlebar.macosButtonPosition.has_value()) {
        setMacOSTitlebarButtonPosition(browser, *style.titlebar.macosButtonPosition);
    }

    // ── Transparency ─────────────────────────────────────────────────────────
    if (style.transparent || style.backgroundOpacity < 1.0f) {
        win.opaque = NO;
        CGFloat alpha = style.backgroundOpacity;
        win.backgroundColor = [NSColor colorWithRed: style.backgroundColor.r / 255.0
                                              green: style.backgroundColor.g / 255.0
                                               blue: style.backgroundColor.b / 255.0
                                              alpha: alpha];
    } else {
        win.opaque = YES;
        win.backgroundColor = [NSColor colorWithRed: style.backgroundColor.r / 255.0
                                              green: style.backgroundColor.g / 255.0
                                               blue: style.backgroundColor.b / 255.0
                                              alpha: 1.0];
    }

    // ── Vibrancy ─────────────────────────────────────────────────────────────
    setMacOSVibrancy(browser, style.macosVibrancy);

    // ── Shadow ───────────────────────────────────────────────────────────────
    win.hasShadow = style.shadow.enabled;

    // ── Window behaviour ─────────────────────────────────────────────────────
    win.level = style.alwaysOnTop ? NSFloatingWindowLevel : NSNormalWindowLevel;

    if (!style.minimizable)
        win.styleMask &= ~NSWindowStyleMaskMiniaturizable;
    if (!style.maximizable)
        [[win standardWindowButton:NSWindowZoomButton] setEnabled:NO];

    // ── Corner radius ─────────────────────────────────────────────────────────
    if (style.cornerRadius > 0) {
        setCornerRadius(browser, style.cornerRadius);
    }

    [win display];
}

void setDragRegions(CefRefPtr<CefBrowser> browser,
                    const std::vector<DragRegion>& regions)
{
    // On macOS, dragging is handled by CEF's drag handler.
    // We store drag regions and return them from GetDraggableRegions().
    // The actual plumbing is in BambooClient::GetDraggableRegions().
    // This call just triggers a redraw so CEF picks up the new regions.
    if (browser) browser->GetHost()->NotifyMoveOrResizeStarted();
}

void setCornerRadius(CefRefPtr<CefBrowser> browser, int radius) {
    NSWindow* win = getNSWindow(browser);
    if (!win) return;
    win.contentView.wantsLayer = YES;
    win.contentView.layer.cornerRadius = radius;
    win.contentView.layer.masksToBounds = YES;
    // Also round the window itself when borderless
    if (win.styleMask & NSWindowStyleMaskBorderless) {
        win.backgroundColor = NSColor.clearColor;
        win.opaque = NO;
    }
}

void setMacOSVibrancy(CefRefPtr<CefBrowser> browser, MacOSVibrancy v) {
    NSWindow* win = getNSWindow(browser);
    if (!win) return;

    removeVibrancy(win);
    if (v == MacOSVibrancy::None) return;

    NSVisualEffectView* vev = [[NSVisualEffectView alloc]
        initWithFrame:win.contentView.bounds];
    vev.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    vev.material  = toNSMaterial(v);
    vev.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    vev.state     = NSVisualEffectStateActive;

    [win.contentView addSubview:vev positioned:NSWindowBelow relativeTo:nil];
}

void setWindowsMaterial(CefRefPtr<CefBrowser>, WindowsMaterial) {
    // No-op on macOS
}

void setBackgroundColor(CefRefPtr<CefBrowser> browser, Color c) {
    NSWindow* win = getNSWindow(browser);
    if (!win) return;
    win.backgroundColor = [NSColor colorWithRed: c.r / 255.0
                                          green: c.g / 255.0
                                           blue: c.b / 255.0
                                          alpha: c.a / 255.0];
}

void setTransparent(CefRefPtr<CefBrowser> browser, bool transparent, float opacity) {
    NSWindow* win = getNSWindow(browser);
    if (!win) return;
    win.opaque = !transparent;
    win.alphaValue = opacity;
}

void setMacOSTitlebarButtonPosition(CefRefPtr<CefBrowser> browser,
                                    const TitlebarButtonPosition& pos)
{
    NSWindow* win = getNSWindow(browser);
    if (!win) return;

    // Subclass the window to override button positions (swizzled approach).
    // For simplicity, we use the titlebar accessor and reposition buttons.
    NSButton* close   = [win standardWindowButton:NSWindowCloseButton];
    NSButton* mini    = [win standardWindowButton:NSWindowMiniaturizeButton];
    NSButton* zoom    = [win standardWindowButton:NSWindowZoomButton];

    if (!close || !mini || !zoom) return;

    // Reposition: close at (pos.x, pos.y), mini and zoom spaced 20px apart
    close.frame = NSMakeRect(pos.x,      pos.y, close.frame.size.width, close.frame.size.height);
    mini.frame  = NSMakeRect(pos.x + 20, pos.y, mini.frame.size.width,  mini.frame.size.height);
    zoom.frame  = NSMakeRect(pos.x + 40, pos.y, zoom.frame.size.width,  zoom.frame.size.height);
}

void setShadow(CefRefPtr<CefBrowser> browser, const Shadow& shadow) {
    NSWindow* win = getNSWindow(browser);
    if (!win) return;
    win.hasShadow = shadow.enabled;
    [win invalidateShadow];
}

void setResizable(CefRefPtr<CefBrowser> browser, bool resizable) {
    NSWindow* win = getNSWindow(browser);
    if (!win) return;
    if (resizable)
        win.styleMask |= NSWindowStyleMaskResizable;
    else
        win.styleMask &= ~NSWindowStyleMaskResizable;
}

} // namespace bamboo::platform
