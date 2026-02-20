// bamboo/App.cpp
#include "bamboo/App.hpp"
#include "bamboo/JsBridge.hpp"
#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"
#include <print>
#include <format>

#if defined(_WIN32)
  #include <windows.h>
#elif defined(__APPLE__)
  #include "include/wrapper/cef_library_loader.h"
#endif

namespace bamboo {

// ─── Internal CEF app ─────────────────────────────────────────────────────────

class BambooCefApp final
    : public CefApp,
      public CefBrowserProcessHandler
{
public:
    explicit BambooCefApp(AppConfig cfg) : config_(std::move(cfg)) {}

    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }

    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
        return jsBridge_;
    }

    void OnBeforeCommandLineProcessing(const CefString&,
                                       CefRefPtr<CefCommandLine> cmd) override
    {
        if (!config_.enableGPU)              cmd->AppendSwitch("disable-gpu");
        if (!config_.enableWebGL)            cmd->AppendSwitch("disable-webgl");
        if (config_.ignoreCertificateErrors) cmd->AppendSwitch("ignore-certificate-errors");
        for (const auto& flag : config_.chromiumFlags) {
            cmd->AppendSwitch(flag.starts_with("--") ? flag.substr(2) : flag);
        }
    }

    void OnContextInitialized() override {
        std::println("[Bamboo] Chromium context initialized.");
    }

    IMPLEMENT_REFCOUNTING(BambooCefApp);
private:
    AppConfig config_;
    CefRefPtr<BambooJsBridge> jsBridge_ = new BambooJsBridge();
};

class BambooSubprocessApp final : public CefApp {
public:
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return jsBridge_; }
    IMPLEMENT_REFCOUNTING(BambooSubprocessApp);
private:
    CefRefPtr<BambooJsBridge> jsBridge_ = new BambooJsBridge();
};

// ─── App ──────────────────────────────────────────────────────────────────────

App::App(AppConfig config) : config_(std::move(config)) {}

App::~App() {
    CefShutdown();
    std::println("[Bamboo] Shutdown complete.");
}

std::expected<std::unique_ptr<App>, AppError>
App::create(int argc, char* argv[], AppConfig config) {
    if (config.userAgent.empty())
        config.userAgent = std::format("{}/{} Bamboo/{}", config.name, config.version, App::version());

#if defined(__APPLE__)
    std::string fwkPath = "./Bamboo.app/Contents/Frameworks/"
                          "Chromium Embedded Framework.framework/"
                          "Chromium Embedded Framework";
    if (!CefLoadLibrary(fwkPath.c_str()))
        return std::unexpected(AppError::InitFailed);
#endif

    CefMainArgs mainArgs(
#if defined(_WIN32)
        GetModuleHandle(nullptr)
#else
        argc, argv
#endif
    );

    auto subApp = CefRefPtr<BambooSubprocessApp>(new BambooSubprocessApp());
    int exitCode = CefExecuteProcess(mainArgs, subApp, nullptr);
    if (exitCode >= 0) std::exit(exitCode);

    auto app = std::unique_ptr<App>(new App(config));
    app->cefApp_ = new BambooCefApp(config);

    CefSettings settings;
    settings.no_sandbox = 1;
    settings.log_severity = config.logToConsole ? LOGSEVERITY_INFO : LOGSEVERITY_DISABLE;
    settings.remote_debugging_port = config.remoteDebugging ? config.remoteDebugPort : 0;
    CefString(&settings.cache_path).FromString(config.cachePath);
    CefString(&settings.log_file).FromString(config.logPath);
    CefString(&settings.user_agent).FromString(config.userAgent);

    if (!CefInitialize(mainArgs, settings, app->cefApp_, nullptr))
        return std::unexpected(AppError::InitFailed);

    std::println("[Bamboo] v{} initialized.", App::version());
    if (config.remoteDebugging)
        std::println("[Bamboo] DevTools: http://localhost:{}", config.remoteDebugPort);

    return app;
}

void App::run()   { CefRunMessageLoop(); }
void App::quit()  { CefQuitMessageLoop(); }
bool App::isUIThread() const { return CefCurrentlyOn(TID_UI); }
void App::postUITask(std::function<void()> task) {
    CefPostTask(TID_UI, CefCreateClosureTask(std::move(task)));
}

} // namespace bamboo
