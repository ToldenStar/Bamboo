#pragma once
// bamboo/Browser.hpp
// Browser window — the core of Bamboo.

#include "bamboo/WindowStyle.hpp"
#include <string>
#include <string_view>
#include <functional>
#include <memory>
#include <expected>
#include <optional>
#include <variant>
#include <vector>
#include <unordered_map>

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"
#include "include/cef_display_handler.h"
#include "include/cef_request_handler.h"
#include "include/cef_context_menu_handler.h"
#include "include/cef_drag_handler.h"
#include "include/cef_keyboard_handler.h"
#include "include/cef_find_handler.h"

namespace bamboo {

// ─── JS value type ────────────────────────────────────────────────────────────

using JsValue = std::variant<std::monostate, bool, double, std::string>;

// ─── Window config ────────────────────────────────────────────────────────────

struct WindowConfig {
    // Identity
    std::string title   = "Bamboo App";
    std::string url     = "about:blank";

    // Geometry
    int  width          = 1280;
    int  height         = 800;
    int  minWidth       = 400;
    int  minHeight      = 300;
    int  maxWidth       = 0;   // 0 = unlimited
    int  maxHeight      = 0;
    int  x              = -1;  // -1 = centered
    int  y              = -1;

    // Style (see WindowStyle.hpp for the full range of options)
    WindowStyle style;
};

// ─── Error codes ──────────────────────────────────────────────────────────────

enum class BrowserError {
    CreateFailed,
    InvalidState,
    JSException,
    NavigationBlocked,
};

// ─── Event structs ────────────────────────────────────────────────────────────

struct LoadEvent {
    std::string url;
    int         httpStatus;
    bool        isError;
    std::string errorText;
};

struct ConsoleEvent {
    enum class Level { Debug, Info, Warning, Error };
    Level       level;
    std::string message;
    std::string source;
    int         line;
};

struct FindResult {
    int  identifier;
    int  count;
    bool finalUpdate;
};

struct NavigationRequest {
    std::string url;
    bool        isRedirect;
    bool        isMainFrame;
    // Return true to allow, false to block.
    bool        allow = true;
};

// ─── Forward declaration ──────────────────────────────────────────────────────

class BambooClient;

/**
 * @brief A Bamboo browser window.
 *
 * The main abstraction over a CEF browser. Provides:
 *   - Navigation + page lifecycle
 *   - JS ↔ C++ bidirectional bridge
 *   - Deep GUI customization (chrome, transparency, vibrancy, drag regions)
 *   - Find-in-page, zoom, print, screenshot
 *   - Navigation request interception
 */
class Browser : public std::enable_shared_from_this<Browser> {
public:
    ~Browser();

    Browser(const Browser&)            = delete;
    Browser& operator=(const Browser&) = delete;
    Browser(Browser&&)                 = default;
    Browser& operator=(Browser&&)      = default;

    // ── Factory ──────────────────────────────────────────────────────────────

    [[nodiscard]]
    static std::expected<std::shared_ptr<Browser>, BrowserError>
    create(WindowConfig config = {});

    // ── Navigation ───────────────────────────────────────────────────────────

    void navigate(std::string_view url);
    void reload(bool ignoreCache = false);
    void goBack();
    void goForward();
    void stop();

    [[nodiscard]] std::string currentURL()  const;
    [[nodiscard]] std::string currentTitle()const;
    [[nodiscard]] bool        isLoading()   const;
    [[nodiscard]] bool        canGoBack()   const;
    [[nodiscard]] bool        canGoForward()const;

    // ── JavaScript bridge ─────────────────────────────────────────────────────

    /** Fire-and-forget JS execution. */
    void executeJS(std::string_view script);

    /** Evaluate JS and receive the typed result asynchronously. */
    void evalJS(std::string_view script,
                std::function<void(std::expected<JsValue, BrowserError>)> callback);

    /**
     * Bind a C++ function callable from JS:
     *   const result = await window.bamboo.call('myFunc', arg1, arg2);
     */
    void bindFunction(std::string name,
                      std::function<JsValue(std::vector<JsValue>)> handler);

    /**
     * Send a pub/sub message to JS:
     *   window.bamboo.on('event', data => { ... });
     */
    void sendMessage(std::string_view event, std::string_view jsonPayload = "null");

    // ── GUI customization ─────────────────────────────────────────────────────

    /**
     * @brief Apply a WindowStyle at any time — even after the window is shown.
     *
     * This is the main entry point for runtime style changes.
     * Handles: transparency, vibrancy, titlebar, chrome mode, drag regions,
     * corner radius, material effects, zoom, scrollbar style, and more.
     */
    void setStyle(const WindowStyle& style);

    /** Access the current effective style. */
    [[nodiscard]] const WindowStyle& style() const { return config_.style; }

    /**
     * @brief Update individual drag regions (frameless windows).
     *
     * Replaces the current set. Regions are relative to the browser content area.
     * Sending an empty vector clears all drag regions.
     */
    void setDragRegions(std::vector<DragRegion> regions);

    /** Apply macOS vibrancy material. No-op on other platforms. */
    void setMacOSVibrancy(MacOSVibrancy v);

    /** Apply Windows 11 Mica / Acrylic material. No-op on other platforms. */
    void setWindowsMaterial(WindowsMaterial m);

    /** Set window background color (visible during load before HTML renders). */
    void setBackgroundColor(Color c);

    /** Set the window corner radius (Windows 11 / macOS). */
    void setCornerRadius(int radius);

    /** Toggle OS-level window shadow. */
    void setShadow(const Shadow& shadow);

    // ── Chrome / window decoration ────────────────────────────────────────────

    /** Switch between Full browser chrome, minimal frame, or frameless. */
    void setChromeMode(ChromeMode mode);

    /** Customize the titlebar when using CustomTitlebar or NativeTitlebar mode. */
    void setTitlebarStyle(const TitlebarStyle& ts);

    // ── Window control ────────────────────────────────────────────────────────

    void show();
    void hide();
    void close();
    void focus();
    void minimize();
    void maximize();
    void restore();
    void center();
    void resize(int width, int height);
    void move(int x, int y);
    void setMinSize(int w, int h);
    void setMaxSize(int w, int h);
    void setTitle(std::string_view title);
    void setAlwaysOnTop(bool value);
    void setFullscreen(bool value);

    /** Open Chromium DevTools in its own window, or docked. */
    void openDevTools(bool docked = false);
    void closeDevTools();

    // ── Zoom ─────────────────────────────────────────────────────────────────

    void setZoom(float factor);       // 1.0 = 100%
    void zoomIn();
    void zoomOut();
    void resetZoom();
    [[nodiscard]] float zoom() const;

    // ── Find in page ─────────────────────────────────────────────────────────

    void findText(std::string_view text, bool forward = true, bool caseSensitive = false);
    void clearFind();

    // ── Screenshot ───────────────────────────────────────────────────────────

    /** Capture the current viewport as a PNG, returned as raw bytes. */
    void captureScreenshot(std::function<void(std::vector<uint8_t> pngBytes)> callback);

    // ── Print ─────────────────────────────────────────────────────────────────

    void print();
    void printToPDF(std::string_view outputPath,
                    std::function<void(bool success)> callback = {});

    // ── Events ────────────────────────────────────────────────────────────────

    using LoadCallback         = std::function<void(const LoadEvent&)>;
    using TitleCallback        = std::function<void(std::string_view)>;
    using CloseCallback        = std::function<void()>;
    using ConsoleCallback      = std::function<void(const ConsoleEvent&)>;
    using MessageCallback      = std::function<void(std::string_view event,
                                                    std::string_view jsonData)>;
    using NavigationCallback   = std::function<void(NavigationRequest&)>;
    using FindCallback         = std::function<void(const FindResult&)>;
    using FocusCallback        = std::function<void(bool gained)>;
    using StyleChangeCallback  = std::function<void(const WindowStyle&)>;

    void onLoad(LoadCallback cb);
    void onTitleChange(TitleCallback cb);
    void onClose(CloseCallback cb);
    void onConsole(ConsoleCallback cb);
    void onMessage(MessageCallback cb);

    /**
     * Called before every navigation. Set request.allow = false to block.
     * Useful for implementing single-origin policies.
     */
    void onNavigation(NavigationCallback cb);

    void onFind(FindCallback cb);
    void onFocusChange(FocusCallback cb);

    /**
     * Called when JS calls window.bamboo.setStyle({...}).
     * Lets your C++ code respond to style change requests from the web layer.
     */
    void onStyleChange(StyleChangeCallback cb);

    // ── Internals ─────────────────────────────────────────────────────────────

    [[nodiscard]] CefRefPtr<CefBrowser> cefBrowser() const { return cefBrowser_; }
    void setCefBrowser(CefRefPtr<CefBrowser> b);

    void fireLoad(LoadEvent e);
    void fireTitleChange(std::string title);
    void fireClose();
    void fireConsole(ConsoleEvent e);
    void fireMessage(std::string_view event, std::string_view json);
    void fireNavigation(NavigationRequest& req);
    void fireFocus(bool gained);

private:
    explicit Browser(WindowConfig config);
    void applyStyleToPlatform(const WindowStyle& style);
    void injectBridgeCSS();

    WindowConfig  config_;
    CefRefPtr<CefBrowser>     cefBrowser_;
    CefRefPtr<BambooClient>   client_;
    float                     zoomLevel_ = 1.0f;

    LoadCallback       onLoad_;
    TitleCallback      onTitleChange_;
    CloseCallback      onClose_;
    ConsoleCallback    onConsole_;
    MessageCallback    onMessage_;
    NavigationCallback onNavigation_;
    FindCallback       onFind_;
    FocusCallback      onFocusChange_;
    StyleChangeCallback onStyleChange_;

    std::unordered_map<int, std::function<void(std::expected<JsValue, BrowserError>)>>
        pendingCallbacks_;
    int nextCallbackId_ = 0;

    std::unordered_map<std::string, std::function<JsValue(std::vector<JsValue>)>>
        boundFunctions_;
};

// ─── CEF client ───────────────────────────────────────────────────────────────

class BambooClient final
    : public CefClient,
      public CefLifeSpanHandler,
      public CefLoadHandler,
      public CefDisplayHandler,
      public CefContextMenuHandler,
      public CefRequestHandler,
      public CefKeyboardHandler,
      public CefFindHandler
{
public:
    explicit BambooClient(std::shared_ptr<Browser> owner)
        : owner_(std::move(owner)) {}

    // CefClient routing
    CefRefPtr<CefLifeSpanHandler>    GetLifeSpanHandler()    override { return this; }
    CefRefPtr<CefLoadHandler>        GetLoadHandler()         override { return this; }
    CefRefPtr<CefDisplayHandler>     GetDisplayHandler()      override { return this; }
    CefRefPtr<CefContextMenuHandler> GetContextMenuHandler()  override { return this; }
    CefRefPtr<CefRequestHandler>     GetRequestHandler()      override { return this; }
    CefRefPtr<CefKeyboardHandler>    GetKeyboardHandler()     override { return this; }
    CefRefPtr<CefFindHandler>        GetFindHandler()         override { return this; }

    // Lifespan
    void OnAfterCreated(CefRefPtr<CefBrowser> browser)                         override;
    bool DoClose(CefRefPtr<CefBrowser> browser)                                override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser)                          override;

    // Load
    void OnLoadEnd(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, int httpStatus) override;
    void OnLoadError(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, ErrorCode,
                     const CefString& errorText, const CefString& failedUrl)  override;

    // Display
    void OnTitleChange(CefRefPtr<CefBrowser>, const CefString& title)          override;
    bool OnConsoleMessage(CefRefPtr<CefBrowser>, cef_log_severity_t,
                          const CefString& message,
                          const CefString& source, int line)                   override;
    void OnGotFocus(CefRefPtr<CefBrowser>)                                     override;

    // Context menu
    void OnBeforeContextMenu(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
                             CefRefPtr<CefContextMenuParams>,
                             CefRefPtr<CefMenuModel> model)                    override;

    // Navigation
    bool OnBeforeBrowse(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
                        CefRefPtr<CefRequest>, bool isRedirect, bool isNav)    override;

    // Find
    void OnFindResult(CefRefPtr<CefBrowser>, int identifier,
                      int count, const CefRect&, int, bool finalUpdate)        override;

    IMPLEMENT_REFCOUNTING(BambooClient);

private:
    std::shared_ptr<Browser> owner_;
};

} // namespace bamboo
