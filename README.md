# 🎋 Bamboo

A modern **C++23** desktop application framework powered by the
[Chromium Embedded Framework (CEF)](https://bitbucket.org/chromiumembedded/cef).
Build cross-platform desktop apps with a Chromium web UI

---

## Features

| Feature | Details |
|---|---|
| **Auto-fetch CEF** | CMake downloads the right CEF binary for your OS + arch automatically |
| **macOS .app bundle** | Full bundle with helpers, framework symlinks, Info.plist, ad-hoc signing |
| **C++23** | `std::expected`, `std::format`, `std::print`, concepts throughout |
| **Default Chrome UI** | `ChromeMode::Full` gives you a complete Chrome browser window |
| **Deep GUI customization** | Frameless, transparent, vibrancy (macOS), Mica (Win11), corner radius, drag regions |
| **JS ↔ C++ bridge** | Pub/sub messages, bound function calls, `evalJS`, `sendMessage` |
| **Navigation interception** | Block or redirect any navigation request |
| **Find-in-page, zoom, print** | All built-in |
| **Remote DevTools** | `chrome://inspect` integration |

---

## Build (no manual CEF download needed!)

```bash
git clone https://github.com/you/bamboo
cd bamboo
mkdir build && cd build
cmake ..                   # downloads CEF automatically on first run
cmake --build . --config Release
./bamboo_demo              # or open Bamboo Demo.app on macOS
```

> **First build** downloads ~500 MB of CEF binaries. Subsequent builds use the cache.

---

## Quick Start

```cpp
#include "bamboo/App.hpp"
#include "bamboo/Browser.hpp"

int main(int argc, char* argv[]) {
    // MUST be first — handles CEF's sub-process bootstrap
    auto app = bamboo::App::create(argc, argv).value();

    auto win = bamboo::Browser::create({
        .title = "My App",
        .url   = "https://example.com",
        .width = 1280,
        .height = 800,
    }).value();

    win->onClose([&]{ app->quit(); });
    app->run();
}
```

---

## Window Styles

### Preset: Full Chrome browser UI
```cpp
auto style = bamboo::WindowStyle::fullBrowser();
// Gives you address bar, tabs, back/forward — literally Chrome
```

### Preset: 100% custom (frameless + transparent)
```cpp
auto style = bamboo::WindowStyle::fullCustom();
// No OS chrome at all — your HTML/CSS is everything
```

### Preset: macOS modern (hidden titlebar + vibrancy)
```cpp
auto style = bamboo::WindowStyle::macosModern(bamboo::MacOSVibrancy::Sidebar);
// Traffic lights float over web content; window material blurs desktop
```

### Preset: Windows 11 Mica
```cpp
auto style = bamboo::WindowStyle::windows11Mica();
// Frosted glass material effect
```

### Custom style (mix any properties)
```cpp
bamboo::WindowStyle style;
style.chromeMode         = bamboo::ChromeMode::CustomTitlebar;
style.titlebar.macosHidden = true;         // macOS traffic lights over content
style.macosVibrancy      = bamboo::MacOSVibrancy::Sidebar;
style.windowsMaterial    = bamboo::WindowsMaterial::Mica;
style.backgroundOpacity  = 0.92f;
style.cornerRadius       = 12;
style.scrollbar          = bamboo::ScrollbarStyle::Overlay;
style.contextMenu        = bamboo::ContextMenuStyle::Custom;
style.dragRegions        = {{ 0, 0, 9999, 38, true }};  // top 38px is draggable
```

### Runtime style changes (including from JavaScript!)
```cpp
// From C++
win->setCornerRadius(20);
win->setMacOSVibrancy(bamboo::MacOSVibrancy::HudWindow);

// From JavaScript
window.bamboo.setStyle({ cornerRadius: 20, transparent: true });
window.bamboo.setZoom(1.5);
window.bamboo.setDragRegions([{ x: 0, y: 0, width: 1280, height: 40 }]);
```

---

## JS ↔ C++ Bridge

### C++ → JS
```cpp
win->sendMessage("userLoggedIn", R"({"name": "Alice"})");
```
```js
window.bamboo.on('userLoggedIn', data => console.log('Welcome', data.name));
```

### JS → C++
```js
window.bamboo.send('buttonClicked', { id: 'submit' });
```
```cpp
win->onMessage([](std::string_view event, std::string_view json) { ... });
```

### JS calls a C++ function (async, returns a Promise)
```cpp
win->bindFunction("add", [](std::vector<bamboo::JsValue> args) -> bamboo::JsValue {
    return std::get<double>(args[0]) + std::get<double>(args[1]);
});
```
```js
const result = await window.bamboo.call('add', 3, 4); // → 7
```

### C++ evaluates JS, gets the result
```cpp
win->evalJS("document.title", [](auto result) {
    std::println("Title: {}", std::get<std::string>(*result));
});
```

### Full window.bamboo JS API
```js
window.bamboo.version          // "1.0.0"
window.bamboo.platform         // "windows" | "macos" | "linux"
window.bamboo.send(event, data)
window.bamboo.on(event, cb)    // returns unsubscribe fn
window.bamboo.off(event, cb)
window.bamboo.call(name, ...args)       // → Promise
window.bamboo.setStyle({...})           // → Promise
window.bamboo.setDragRegions([...])
window.bamboo.minimize() / maximize() / restore() / close()
window.bamboo.setTitle('New Title')
window.bamboo.setZoom(1.5)
window.bamboo.setAlwaysOnTop(true)
window.bamboo.setFullscreen(true)
window.bamboo.openDevTools()
window.bamboo.print()
window.bamboo.captureScreenshot()       // → Promise<base64 PNG>
```

---

## File Structure

```
bamboo/
├── cmake/
│   ├── FetchCEF.cmake              ← auto-downloads CEF for your platform
│   ├── BambooBundleMacOS.cmake     ← assembles macOS .app bundle
│   ├── CreateHelpers.cmake         ← creates CEF helper .app bundles
│   └── Info.plist.in               ← macOS Info.plist template
├── include/bamboo/
│   ├── App.hpp                     ← app lifecycle
│   ├── Browser.hpp                 ← browser window + events
│   ├── WindowStyle.hpp             ← all GUI customization types
│   ├── JsBridge.hpp                ← window.bamboo JS injection
│   └── platform/
│       └── StyleApplicator.hpp     ← platform style API
├── src/
│   ├── App.cpp
│   ├── Browser.cpp
│   └── platform/
│       ├── StyleApplicator_mac.mm  ← AppKit / NSWindow
│       ├── StyleApplicator_win.cpp ← DWM / Win32
│       └── StyleApplicator_linux.cpp ← GTK3 / X11
├── examples/
│   └── main.cpp                    ← full demo
└── CMakeLists.txt
```

---

## Platform Notes

**macOS** — Requires Xcode command-line tools. CEF is a `.framework` inside the `.app` bundle.
Apple Silicon and Intel are both supported (auto-detected).

**Windows** — MSVC 2022 (v17.8+) or Clang-cl 17+. DWM APIs used for Mica/Acrylic/shadow.
Windows 11 22H2+ for full Mica support; falls back to Acrylic on older builds.

**Linux** — GCC 13+ or Clang 17+. Requires GTK3 and a display (X11; XWayland for Wayland).
`sudo apt install libgtk-3-dev` if not already present.

---

## CEF Version

The CEF version is pinned in `cmake/FetchCEF.cmake`:
```cmake
set(BAMBOO_CEF_VERSION "122.1.10+..." CACHE STRING "CEF version")
```
Override at configure time: `cmake .. -DBAMBOO_CEF_VERSION=<version>`
Browse available versions at https://cef-builds.spotifycdn.com/index.html
