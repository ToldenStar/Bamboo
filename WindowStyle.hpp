#pragma once
// bamboo/WindowStyle.hpp
// Deep GUI customization for Bamboo windows.
// Controls everything from native chrome to custom titlebars, themes,
// transparency, vibrancy, and pixel-level window decorations.

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace bamboo {

// ─── Color ────────────────────────────────────────────────────────────────────

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;

    static constexpr Color rgb(uint8_t r, uint8_t g, uint8_t b) {
        return {r, g, b, 255};
    }
    static constexpr Color rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        return {r, g, b, a};
    }

    /**
     * Construct from a 32-bit ARGB hex literal, e.g. Color::hex(0xFF'FF'00'00).
     * Layout: bits [31:24] = alpha, [23:16] = red, [15:8] = green, [7:0] = blue.
     */
    static constexpr Color hex(uint32_t argb) {
        return {
            static_cast<uint8_t>((argb >> 16) & 0xFF),  // red
            static_cast<uint8_t>((argb >>  8) & 0xFF),  // green
            static_cast<uint8_t>((argb >>  0) & 0xFF),  // blue
            static_cast<uint8_t>((argb >> 24) & 0xFF),  // alpha
        };
    }

    static constexpr Color transparent() { return {0, 0, 0, 0}; }
    static constexpr Color white()       { return {255, 255, 255, 255}; }
    static constexpr Color black()       { return {0, 0, 0, 255}; }
};

// ─── Chrome UI Mode ───────────────────────────────────────────────────────────

enum class ChromeMode {
    /**
     * Full native Chromium browser UI — address bar, tabs, back/forward,
     * bookmarks bar, everything. Identical to Google Chrome.
     */
    Full,

    /**
     * Minimal native frame: just the system titlebar and window borders.
     * No address bar, tabs, or toolbar. You supply the UI via HTML/CSS/JS.
     */
    NativeTitlebar,

    /**
     * Completely frameless window — no OS chrome at all.
     * You control 100% of the UI. Requires manual drag regions (see DragRegion).
     */
    Frameless,

    /**
     * Custom titlebar: OS-provided window controls (traffic lights on macOS,
     * min/max/close on Windows) but the rest is your HTML.
     * On macOS this enables the "hidden titlebar" style.
     */
    CustomTitlebar,
};

// ─── macOS-specific vibrancy ──────────────────────────────────────────────────

enum class MacOSVibrancy {
    None,
    Sidebar,
    Menu,
    Popover,
    HudWindow,
    UnderWindowBackground,
    UnderPageBackground,
    Titlebar,
    HeaderView,
    Sheet,
    WindowBackground,
    ContentBackground,
    FullScreenUI,  // macOS 14+
};

// ─── Windows-specific Mica / Acrylic ─────────────────────────────────────────

enum class WindowsMaterial {
    None,
    Mica,     // Windows 11 — blurs content behind the window
    MicaAlt,  // Windows 11 — darker variant
    Acrylic,  // Windows 10+ — stronger blur with tint
    Tabbed,   // Windows 11 — multi-tab Mica
};

// ─── Drag regions (frameless windows) ────────────────────────────────────────

struct DragRegion {
    int  x, y, width, height;
    bool isDraggable = true;  // false = a no-drag "hole" punched inside a drag rect
};

// ─── Window shadow ────────────────────────────────────────────────────────────

struct Shadow {
    bool  enabled = true;
    Color color   = Color::rgba(0, 0, 0, 80);
    int   blur    = 20;
    int   spread  = 0;
    int   offsetX = 0;
    int   offsetY = 4;
};

// ─── Traffic light / titlebar button position (macOS) ────────────────────────

struct TitlebarButtonPosition {
    int x = 20;  // pixels from left edge
    int y = 20;  // pixels from top edge
};

// ─── Context menu style ───────────────────────────────────────────────────────

enum class ContextMenuStyle {
    Default,   // Native OS context menu
    Custom,    // Route to JS (window.bamboo.onContextMenu)
    Disabled,  // No context menu
};

// ─── Scrollbar style ──────────────────────────────────────────────────────────

enum class ScrollbarStyle {
    Default,  // OS default
    Hidden,   // Always hidden
    Overlay,  // Thin overlay scrollbar (macOS style)
};

// ─── Fullscreen behaviour ─────────────────────────────────────────────────────

enum class FullscreenMode {
    Disabled,
    Native,  // OS-level fullscreen
    Kiosk,   // True kiosk: hides taskbar/dock/menubar
};

// ─── Titlebar style ───────────────────────────────────────────────────────────

struct TitlebarStyle {
    bool        visible    = true;
    std::string title      = "";               // empty = use page <title>
    Color       background = Color::rgb(245, 245, 245);
    Color       foreground = Color::black();
    int         height     = 38;               // pixels
    bool        showTitle  = true;
    bool        showIcon   = false;
    std::string iconPath   = "";               // path to .png

    bool transparentWhenInactive = false;

    // macOS "hidden titlebar" — traffic lights float above web content.
    // When true, set height = 0 and visible = false; the OS still renders
    // the window controls but your HTML canvas fills the entire frame.
    bool                                  macosHidden        = false;
    std::optional<TitlebarButtonPosition> macosButtonPosition;
};

// ─── Main window style config ─────────────────────────────────────────────────

struct WindowStyle {
    // ── Chrome / Frame ───────────────────────────────────────────────────────
    ChromeMode    chromeMode = ChromeMode::NativeTitlebar;
    TitlebarStyle titlebar;

    // ── Background ───────────────────────────────────────────────────────────
    Color backgroundColor   = Color::white();

    // Opacity of the window background (0.0–1.0).
    // Setting this below 1.0 alone does NOT make the window transparent to the
    // OS compositor; you must also set `transparent = true` for per-pixel alpha.
    float backgroundOpacity = 1.0f;

    // When true the OS composites the window with per-pixel alpha from HTML.
    // Pair with backgroundOpacity < 1.0 or a transparent CSS background.
    bool transparent = false;

    // ── Platform materials ───────────────────────────────────────────────────
    MacOSVibrancy   macosVibrancy   = MacOSVibrancy::None;
    WindowsMaterial windowsMaterial = WindowsMaterial::None;

    // ── Shadow ───────────────────────────────────────────────────────────────
    Shadow shadow;

    // ── Corner radius (Windows 11 / macOS) ───────────────────────────────────
    int cornerRadius = 0;  // 0 = OS default; >0 = custom rounded corners

    // ── Resize / interaction ─────────────────────────────────────────────────
    bool           resizable   = true;
    bool           minimizable = true;
    bool           maximizable = true;
    bool           alwaysOnTop = false;
    bool           skipTaskbar = false;  // don't show in taskbar/dock
    FullscreenMode fullscreen  = FullscreenMode::Native;

    // ── Drag regions (only used when chromeMode == Frameless) ────────────────
    std::vector<DragRegion> dragRegions;

    // ── Scrollbar ────────────────────────────────────────────────────────────
    ScrollbarStyle scrollbar = ScrollbarStyle::Default;

    // ── Context menu ─────────────────────────────────────────────────────────
    ContextMenuStyle contextMenu = ContextMenuStyle::Default;

    // ── Dev tools ────────────────────────────────────────────────────────────
    bool devTools       = false;
    bool devToolsDocked = false;  // true = docked panel; false = separate window

    // ── Zoom ─────────────────────────────────────────────────────────────────
    float zoomFactor = 1.0f;
    bool  allowZoom  = true;

    // ── Selection ────────────────────────────────────────────────────────────
    bool allowTextSelection = true;

    // ─── Convenience presets ─────────────────────────────────────────────────

    /** Full Chromium browser experience — like opening Chrome. */
    [[nodiscard]] static WindowStyle fullBrowser() {
        WindowStyle s;
        s.chromeMode = ChromeMode::Full;
        return s;
    }

    /**
     * Frameless, transparent window — build 100% custom UI in HTML/CSS.
     * Add DragRegion entries to make portions of the window draggable.
     */
    [[nodiscard]] static WindowStyle fullCustom() {
        WindowStyle s;
        s.chromeMode        = ChromeMode::Frameless;
        s.transparent       = true;
        s.backgroundOpacity = 0.0f;
        s.shadow.enabled    = false;
        s.scrollbar         = ScrollbarStyle::Hidden;
        s.contextMenu       = ContextMenuStyle::Disabled;
        return s;
    }

    /**
     * macOS-native "hidden titlebar" — OS traffic lights float above a full
     * web canvas; the rest of the frame uses the specified vibrancy material.
     */
    [[nodiscard]] static WindowStyle macosModern(
        MacOSVibrancy vibrancy = MacOSVibrancy::WindowBackground)
    {
        WindowStyle s;
        s.chromeMode               = ChromeMode::CustomTitlebar;
        s.titlebar.macosHidden     = true;
        s.titlebar.visible         = false;
        s.titlebar.height          = 0;
        s.macosVibrancy            = vibrancy;
        s.backgroundOpacity        = 0.85f;
        s.shadow.blur              = 30;
        return s;
    }

    /** Windows 11 Mica — modern frosted-glass look. */
    [[nodiscard]] static WindowStyle windows11Mica() {
        WindowStyle s;
        s.windowsMaterial   = WindowsMaterial::Mica;
        s.backgroundOpacity = 0.0f;
        s.transparent       = true;
        return s;
    }
};

// ─── Runtime style change callback ───────────────────────────────────────────

/**
 * @brief Implement this to receive style-change events triggered from JS.
 *
 * JS can call:
 *   window.bamboo.setStyle({ cornerRadius: 16, transparent: true })
 *
 * The callback receives the fully-updated WindowStyle after the change is applied.
 */
using StyleChangeCallback = std::function<void(const WindowStyle& updated)>;

} // namespace bamboo
