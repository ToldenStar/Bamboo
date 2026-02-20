// examples/main.cpp — Bamboo framework demo
// Shows: auto-fetched CEF, macOS bundle, GUI customization, JS bridge.

#include "bamboo/App.hpp"
#include "bamboo/Browser.hpp"
#include <print>
#include <thread>
#include <chrono>

int main(int argc, char* argv[]) {

    // 1. Init — MUST be first line of main()
    auto appResult = bamboo::App::create(argc, argv, {
        .name            = "BambooDemo",
        .version         = "1.0.0",
        .cachePath       = "./bamboo_cache",
        .enableGPU       = true,
        .enableMedia     = true,
        .remoteDebugging = true,
        .remoteDebugPort = 9222,
    });

    if (!appResult) {
        std::println(stderr, "Bamboo init failed (code {})",
                     static_cast<int>(appResult.error()));
        return 1;
    }
    auto& app = *appResult;

    // 2. Choose a window style
    //    Uncomment any preset, or build your own below:

    // PRESET A: Full Chrome browser UI (like opening Chrome itself)
    // bamboo::WindowStyle style = bamboo::WindowStyle::fullBrowser();

    // PRESET B: Frameless + transparent — 100% custom HTML/CSS UI
    // bamboo::WindowStyle style = bamboo::WindowStyle::fullCustom();

    // PRESET C: macOS modern (hidden titlebar + sidebar vibrancy)
    // bamboo::WindowStyle style = bamboo::WindowStyle::macosModern();

    // PRESET D: Windows 11 Mica frosted glass
    // bamboo::WindowStyle style = bamboo::WindowStyle::windows11Mica();

    // CUSTOM style — mix and match any property:
    bamboo::WindowStyle style;
    style.chromeMode             = bamboo::ChromeMode::CustomTitlebar;
    style.titlebar.macosHidden   = true;   // traffic lights float over web content
    style.macosVibrancy          = bamboo::MacOSVibrancy::Sidebar;
    style.windowsMaterial        = bamboo::WindowsMaterial::Mica;
    style.backgroundOpacity      = 0.92f;
    style.cornerRadius           = 12;
    style.scrollbar              = bamboo::ScrollbarStyle::Overlay;
    style.contextMenu            = bamboo::ContextMenuStyle::Custom;
    style.shadow                 = { .enabled=true, .blur=24, .offsetY=8 };
    style.dragRegions            = {{ 0, 0, 9999, 38, true }};  // top bar draggable

    // 3. Create window
    auto winResult = bamboo::Browser::create({
        .title     = "Bamboo Demo",
        .url       = "https://example.com",
        .width     = 1280,
        .height    = 800,
        .minWidth  = 640,
        .minHeight = 480,
        .x         = -1,  // centered
        .y         = -1,
        .style     = style,
    });

    if (!winResult) {
        std::println(stderr, "Failed to create window");
        return 1;
    }
    auto& win = *winResult;

    // 4. Page events
    win->onLoad([&](const bamboo::LoadEvent& e) {
        if (!e.isError) {
            std::println("Loaded: {} ({})", e.url, e.httpStatus);
            win->executeJS(R"js(
                const b = document.createElement('div');
                b.textContent = 'Bamboo Active';
                b.style.cssText = 'position:fixed;bottom:10px;right:10px;'
                    + 'background:#000c;color:#7fff00;padding:6px 14px;'
                    + 'border-radius:6px;font:13px monospace;z-index:9999';
                document.body.appendChild(b);
            )js");
        }
    });

    win->onTitleChange([&](std::string_view t) { win->setTitle(t); });

    win->onConsole([](const bamboo::ConsoleEvent& e) {
        std::println("[JS] {}:{} {}", e.source, e.line, e.message);
    });

    // 5. Navigation guard
    win->onNavigation([](bamboo::NavigationRequest& req) {
        if (req.url.contains("blocked-site.example"))
            req.allow = false;
    });

    // 6. JS → C++ messages
    win->onMessage([&](std::string_view event, std::string_view data) {
        std::println("[msg] '{}' data={}", event, data);
        if (event == "__contextMenu")
            win->sendMessage("showContextMenu", R"({"items":["Copy","Paste"]})");
    });

    // 7. Bound C++ functions (callable from JS)
    win->bindFunction("add", [](std::vector<bamboo::JsValue> args) -> bamboo::JsValue {
        return std::get<double>(args[0]) + std::get<double>(args[1]);
    });

    win->bindFunction("greet", [](std::vector<bamboo::JsValue> args) -> bamboo::JsValue {
        return "Hello, " + std::get<std::string>(args[0]) + "!";
    });

    win->bindFunction("toggleDark", [&](std::vector<bamboo::JsValue>) -> bamboo::JsValue {
        static bool dark = false; dark = !dark;
        bamboo::WindowStyle s = win->style();
        s.titlebar.background = dark ? bamboo::Color::rgb(20,20,20)
                                     : bamboo::Color::rgb(245,245,245);
        win->setStyle(s);
        return dark;
    });

    // 8. evalJS
    win->evalJS("navigator.userAgent", [](auto r) {
        if (r && std::holds_alternative<std::string>(*r))
            std::println("UA: {}", std::get<std::string>(*r));
    });

    // 9. Push a message from a background thread after 3s
    std::thread([&]() {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(3s);
        app->postUITask([&]() {
            win->sendMessage("toast", R"({"text":"Bamboo says hi from C++!"})");
        });
    }).detach();

    // 10. Style changes from JS:
    //   window.bamboo.setStyle({ cornerRadius: 20 })
    //   window.bamboo.setZoom(1.5)
    //   window.bamboo.minimize()
    //   window.bamboo.setDragRegions([{x:0,y:0,width:1280,height:40}])
    win->onStyleChange([](const bamboo::WindowStyle& s) {
        std::println("Style changed from JS — cornerRadius={}", s.cornerRadius);
    });

    // 11. Quit on close
    win->onClose([&]() { app->quit(); });

    std::println("Bamboo running. DevTools: http://localhost:9222");
    app->run();
    return 0;
}
