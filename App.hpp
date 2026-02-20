#pragma once
// bamboo/App.hpp
// Top-level application lifecycle for the Bamboo framework.

#include <string>
#include <functional>
#include <memory>
#include <expected>
#include <span>
#include "include/cef_app.h"
#include "include/cef_base.h"

namespace bamboo {

// ─── Error codes ──────────────────────────────────────────────────────────────

enum class AppError {
    InitFailed,
    AlreadyRunning,
    InvalidArguments,
    CEFVersionMismatch,
};

// ─── App configuration ────────────────────────────────────────────────────────

struct AppConfig {
    // Application identity
    std::string name        = "BambooApp";
    std::string version     = "1.0.0";
    std::string userAgent   = "";     // empty = auto ("BambooApp/1.0.0 Bamboo/1.0")

    // Paths
    std::string cachePath       = "./bamboo_cache";
    std::string logPath         = "./bamboo.log";

    // Chromium flags
    bool enableGPU              = true;
    bool enableWebGL            = true;
    bool enableMedia            = true;   // audio/video/webcam
    bool enableNotifications    = false;
    bool ignoreCertificateErrors = false; // ⚠️ dev only

    // Debugging
    bool remoteDebugging        = false;
    int  remoteDebugPort        = 9222;
    bool logToConsole           = true;

    // Extra Chromium command-line switches
    // e.g. { "--disable-web-security", "--allow-running-insecure-content" }
    std::vector<std::string> chromiumFlags;
};

// ─── Forward declarations ─────────────────────────────────────────────────────

class BambooCefApp;

/**
 * @brief Entry point for a Bamboo desktop application.
 *
 * MUST be the very first thing called in main(). Handles CEF's multi-process
 * bootstrapping, so sub-processes exit before any user code runs.
 *
 * Example:
 *   int main(int argc, char* argv[]) {
 *       auto app = bamboo::App::create(argc, argv).value();
 *       auto win = bamboo::Browser::create({ .url = "https://example.com" }).value();
 *       win->onClose([&]{ app->quit(); });
 *       app->run();
 *   }
 */
class App {
public:
    ~App();

    App(const App&)            = delete;
    App& operator=(const App&) = delete;
    App(App&&)                 = default;
    App& operator=(App&&)      = default;

    /**
     * @brief Initialize Bamboo/CEF. Must be called at the very top of main().
     *
     * Returns std::unexpected(AppError) if initialization fails.
     * On success, returns a unique_ptr<App> that owns the CEF lifetime.
     */
    [[nodiscard]]
    static std::expected<std::unique_ptr<App>, AppError>
    create(int argc, char* argv[], AppConfig config = {});

    /**
     * @brief Block and run the Chromium message loop.
     *        Returns when all windows are closed or quit() is called.
     */
    void run();

    /**
     * @brief Quit the message loop and shut down.
     */
    void quit();

    /**
     * @brief Post a callable to the CEF UI thread (thread-safe).
     */
    void postUITask(std::function<void()> task);

    /**
     * @brief Returns true if the caller is on the CEF UI thread.
     */
    [[nodiscard]] bool isUIThread() const;

    /**
     * @brief Access the app config.
     */
    [[nodiscard]] const AppConfig& config() const { return config_; }

    /**
     * @brief Bamboo framework version string.
     */
    [[nodiscard]] static std::string_view version() { return "1.0.0"; }

private:
    explicit App(AppConfig config);

    AppConfig config_;
    CefRefPtr<BambooCefApp> cefApp_;
};

} // namespace bamboo
