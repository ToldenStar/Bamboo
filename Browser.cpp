// bamboo/Browser.cpp - see include/bamboo/Browser.hpp for API docs
#include "bamboo/Browser.hpp"
#include "bamboo/platform/StyleApplicator.hpp"
#include "include/cef_task.h"
#include "include/wrapper/cef_helpers.h"
#include <nlohmann/json.hpp>
#include <format>
#include <print>
#include <cmath>

using json = nlohmann::json;

namespace bamboo {

namespace {

json jsValueToJson(const JsValue& v) {
    return std::visit([]<typename T>(const T& val) -> json {
        if constexpr (std::is_same_v<T, std::monostate>) return nullptr;
        else return val;
    }, v);
}

JsValue jsonToJsValue(const json& j) {
    if (j.is_null())   return std::monostate{};
    if (j.is_bool())   return j.get<bool>();
    if (j.is_number()) return j.get<double>();
    if (j.is_string()) return j.get<std::string>();
    return std::monostate{};
}

std::string buildBridgeCSS(const WindowStyle& style) {
    std::string css;
    switch (style.scrollbar) {
        case ScrollbarStyle::Hidden:
            css += "::-webkit-scrollbar{display:none}*{-ms-overflow-style:none;scrollbar-width:none}";
            break;
        case ScrollbarStyle::Overlay:
            css += "::-webkit-scrollbar{width:8px;height:8px}"
                   "::-webkit-scrollbar-track{background:transparent}"
                   "::-webkit-scrollbar-thumb{background:rgba(0,0,0,.3);border-radius:4px}";
            break;
        default: break;
    }
    if (!style.allowTextSelection)
        css += "*{user-select:none;-webkit-user-select:none}";
    return css;
}

} // namespace

Browser::Browser(WindowConfig config) : config_(std::move(config)) {}

Browser::~Browser() {
    if (cefBrowser_) cefBrowser_->GetHost()->CloseBrowser(true);
}

std::expected<std::shared_ptr<Browser>, BrowserError>
Browser::create(WindowConfig config) {
    CEF_REQUIRE_UI_THREAD();

    auto self   = std::shared_ptr<Browser>(new Browser(config));
    auto client = CefRefPtr<BambooClient>(new BambooClient(self));
    self->client_ = client;

    CefWindowInfo wi;
#if defined(_WIN32)
    wi.SetAsPopup(nullptr, config.title);
    if (config.style.chromeMode == ChromeMode::Frameless) {
        wi.style = WS_POPUP | WS_VISIBLE | (config.style.resizable ? WS_SIZEBOX : 0);
    }
#elif defined(__APPLE__)
    wi.SetAsPopup(nullptr, config.title);
#else
    wi.SetAsChild(0, {0, 0, config.width, config.height});
#endif

    CefBrowserSettings bs;
    if (config.style.transparent)
        bs.background_color = CefColorSetARGB(0,0,0,0);

    auto browser = CefBrowserHost::CreateBrowserSync(wi, client, config.url, bs, nullptr, nullptr);
    if (!browser) return std::unexpected(BrowserError::CreateFailed);

    self->cefBrowser_ = browser;
    platform::applyStyle(browser, config.style);
    return self;
}

void Browser::setCefBrowser(CefRefPtr<CefBrowser> b) { cefBrowser_ = b; }

void Browser::navigate(std::string_view url) {
    if (cefBrowser_) cefBrowser_->GetMainFrame()->LoadURL(std::string(url));
}
void Browser::reload(bool ignoreCache) {
    if (!cefBrowser_) return;
    ignoreCache ? cefBrowser_->ReloadIgnoreCache() : cefBrowser_->Reload();
}
void Browser::goBack()    { if (cefBrowser_ && cefBrowser_->CanGoBack())    cefBrowser_->GoBack(); }
void Browser::goForward() { if (cefBrowser_ && cefBrowser_->CanGoForward()) cefBrowser_->GoForward(); }
void Browser::stop()      { if (cefBrowser_) cefBrowser_->StopLoad(); }

std::string Browser::currentURL()   const { return cefBrowser_ ? cefBrowser_->GetMainFrame()->GetURL().ToString() : ""; }
std::string Browser::currentTitle() const { return {}; }
bool Browser::isLoading()   const { return cefBrowser_ && cefBrowser_->IsLoading(); }
bool Browser::canGoBack()   const { return cefBrowser_ && cefBrowser_->CanGoBack(); }
bool Browser::canGoForward()const { return cefBrowser_ && cefBrowser_->CanGoForward(); }

void Browser::executeJS(std::string_view script) {
    if (!cefBrowser_) return;
    cefBrowser_->GetMainFrame()->ExecuteJavaScript(
        std::string(script), cefBrowser_->GetMainFrame()->GetURL(), 0);
}

void Browser::evalJS(std::string_view script,
                     std::function<void(std::expected<JsValue, BrowserError>)> cb) {
    int id = nextCallbackId_++;
    pendingCallbacks_[id] = std::move(cb);
    executeJS(std::format(R"js(
        (async()=>{{try{{const r=await(async()=>{{return({});}})();
        window.bamboo.send('__evalResult',{{id:{},value:r,error:null}})}}
        catch(e){{window.bamboo.send('__evalResult',{{id:{},value:null,error:e.message}})}}}})();
    )js", script, id, id));
}

void Browser::bindFunction(std::string name, std::function<JsValue(std::vector<JsValue>)> h) {
    boundFunctions_[std::move(name)] = std::move(h);
}

void Browser::sendMessage(std::string_view event, std::string_view payload) {
    executeJS(std::format("window.bamboo._dispatch({},{});", json(event).dump(), payload));
}

void Browser::setStyle(const WindowStyle& style) {
    config_.style = style;
    if (cefBrowser_) {
        platform::applyStyle(cefBrowser_, style);
        injectBridgeCSS();
    }
    if (onStyleChange_) onStyleChange_(style);
}

void Browser::injectBridgeCSS() {
    std::string css = buildBridgeCSS(config_.style);
    if (css.empty()) return;
    // Escape backticks for JS template literal
    std::string escaped;
    for (char c : css) {
        if (c == '`') escaped += "\\`";
        else escaped += c;
    }
    executeJS(std::format(R"js(
        (function(){{var id='__bamboo_s',el=document.getElementById(id);
        if(!el){{el=document.createElement('style');el.id=id;document.head.appendChild(el)}}
        el.textContent=`{}`;}})();
    )js", escaped));
}

void Browser::setDragRegions(std::vector<DragRegion> r) {
    config_.style.dragRegions = std::move(r);
    if (cefBrowser_) platform::setDragRegions(cefBrowser_, config_.style.dragRegions);
}
void Browser::setMacOSVibrancy(MacOSVibrancy v)   { config_.style.macosVibrancy=v;   if(cefBrowser_) platform::setMacOSVibrancy(cefBrowser_,v); }
void Browser::setWindowsMaterial(WindowsMaterial m){ config_.style.windowsMaterial=m; if(cefBrowser_) platform::setWindowsMaterial(cefBrowser_,m); }
void Browser::setBackgroundColor(Color c)          { config_.style.backgroundColor=c; if(cefBrowser_) platform::setBackgroundColor(cefBrowser_,c); }
void Browser::setCornerRadius(int r)               { config_.style.cornerRadius=r;    if(cefBrowser_) platform::setCornerRadius(cefBrowser_,r); }
void Browser::setShadow(const Shadow& s)           { config_.style.shadow=s;          if(cefBrowser_) platform::setShadow(cefBrowser_,s); }
void Browser::setChromeMode(ChromeMode m)          { config_.style.chromeMode=m;      setStyle(config_.style); }
void Browser::setTitlebarStyle(const TitlebarStyle& ts){ config_.style.titlebar=ts;   setStyle(config_.style); }

void Browser::show()     { if (cefBrowser_) cefBrowser_->GetHost()->SetWindowVisibility(true); }
void Browser::hide()     { if (cefBrowser_) cefBrowser_->GetHost()->SetWindowVisibility(false); }
void Browser::close()    { if (cefBrowser_) cefBrowser_->GetHost()->CloseBrowser(false); }
void Browser::focus()    { if (cefBrowser_) cefBrowser_->GetHost()->SetFocus(true); }
void Browser::minimize() { /* platform-specific */ }
void Browser::maximize() { /* platform-specific */ }
void Browser::restore()  { /* platform-specific */ }
void Browser::center()   { /* platform-specific */ }

void Browser::resize(int w, int h) {
#if defined(_WIN32)
    if (cefBrowser_) SetWindowPos(cefBrowser_->GetHost()->GetWindowHandle(),nullptr,0,0,w,h,SWP_NOMOVE|SWP_NOZORDER);
#endif
}
void Browser::move(int x, int y) {
#if defined(_WIN32)
    if (cefBrowser_) SetWindowPos(cefBrowser_->GetHost()->GetWindowHandle(),nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER);
#endif
}
void Browser::setMinSize(int w, int h) { config_.minWidth=w; config_.minHeight=h; }
void Browser::setMaxSize(int w, int h) { config_.maxWidth=w; config_.maxHeight=h; }
void Browser::setTitle(std::string_view t) {
#if defined(_WIN32)
    if (cefBrowser_) SetWindowText(cefBrowser_->GetHost()->GetWindowHandle(), std::string(t).c_str());
#endif
}
void Browser::setAlwaysOnTop(bool v)  { config_.style.alwaysOnTop=v; setStyle(config_.style); }
void Browser::setFullscreen(bool v)   { if (cefBrowser_) cefBrowser_->GetHost()->SetFullscreen(v); }

void Browser::openDevTools(bool docked) {
    if (!cefBrowser_) return;
    CefWindowInfo wi; CefBrowserSettings bs;
    cefBrowser_->GetHost()->ShowDevTools(wi, nullptr, bs, {});
}
void Browser::closeDevTools() { if (cefBrowser_) cefBrowser_->GetHost()->CloseDevTools(); }

void Browser::setZoom(float f) {
    zoomLevel_ = f;
    if (cefBrowser_) cefBrowser_->GetHost()->SetZoomLevel(std::log(f) / std::log(1.2));
}
void Browser::zoomIn()    { setZoom(zoomLevel_ * 1.2f); }
void Browser::zoomOut()   { setZoom(zoomLevel_ / 1.2f); }
void Browser::resetZoom() { setZoom(1.0f); }
float Browser::zoom() const { return zoomLevel_; }

void Browser::findText(std::string_view text, bool forward, bool caseSensitive) {
    if (!cefBrowser_) return;
    CefFindSettings fs; fs.match_case = caseSensitive;
    cefBrowser_->GetHost()->Find(std::string(text), forward, fs, false);
}
void Browser::clearFind() { if (cefBrowser_) cefBrowser_->GetHost()->StopFinding(true); }
void Browser::captureScreenshot(std::function<void(std::vector<uint8_t>)>) { /* windowless mode only */ }
void Browser::print()    { if (cefBrowser_) cefBrowser_->GetHost()->Print(); }
void Browser::printToPDF(std::string_view path, std::function<void(bool)>) {
    if (cefBrowser_) cefBrowser_->GetHost()->PrintToPDF(std::string(path), {}, nullptr);
}

void Browser::onLoad(LoadCallback cb)              { onLoad_        = std::move(cb); }
void Browser::onTitleChange(TitleCallback cb)      { onTitleChange_ = std::move(cb); }
void Browser::onClose(CloseCallback cb)            { onClose_       = std::move(cb); }
void Browser::onConsole(ConsoleCallback cb)        { onConsole_     = std::move(cb); }
void Browser::onMessage(MessageCallback cb)        { onMessage_     = std::move(cb); }
void Browser::onNavigation(NavigationCallback cb)  { onNavigation_  = std::move(cb); }
void Browser::onFind(FindCallback cb)              { onFind_        = std::move(cb); }
void Browser::onFocusChange(FocusCallback cb)      { onFocusChange_ = std::move(cb); }
void Browser::onStyleChange(StyleChangeCallback cb){ onStyleChange_ = std::move(cb); }

void Browser::fireLoad(LoadEvent e)              { if(onLoad_)        onLoad_(e); }
void Browser::fireTitleChange(std::string title) { if(onTitleChange_) onTitleChange_(title); }
void Browser::fireClose()                        { if(onClose_)       onClose_(); }
void Browser::fireConsole(ConsoleEvent e)        { if(onConsole_)     onConsole_(e); }
void Browser::fireFocus(bool gained)             { if(onFocusChange_) onFocusChange_(gained); }
void Browser::fireNavigation(NavigationRequest& req) { if(onNavigation_) onNavigation_(req); }

void Browser::fireMessage(std::string_view event, std::string_view data) {
    if (event == "__evalResult") {
        auto j = json::parse(data, nullptr, false);
        if (j.is_discarded()) return;
        auto it = pendingCallbacks_.find(j["id"].get<int>());
        if (it == pendingCallbacks_.end()) return;
        if (!j["error"].is_null()) it->second(std::unexpected(BrowserError::JSException));
        else it->second(jsonToJsValue(j["value"]));
        pendingCallbacks_.erase(it);
        return;
    }
    if (event == "__call") {
        auto j = json::parse(data, nullptr, false); if(j.is_discarded()) return;
        std::string name=j["name"], id=j["id"];
        auto it = boundFunctions_.find(name);
        if (it == boundFunctions_.end()) {
            executeJS(std::format("window.bamboo._resolveCall({},null,'Unknown: {}');", json(id).dump(), name));
            return;
        }
        std::vector<JsValue> args;
        for (const auto& a : j["args"]) args.push_back(jsonToJsValue(a));
        auto result = it->second(args);
        executeJS(std::format("window.bamboo._resolveCall({},{},null);", json(id).dump(), jsValueToJson(result).dump()));
        return;
    }
    if (event == "__setStyle") {
        auto j = json::parse(data, nullptr, false); if(j.is_discarded()) return;
        auto& s = config_.style;
        if (j.contains("cornerRadius"))      s.cornerRadius       = j["cornerRadius"].get<int>();
        if (j.contains("transparent"))       s.transparent        = j["transparent"].get<bool>();
        if (j.contains("backgroundOpacity")) s.backgroundOpacity  = j["backgroundOpacity"].get<float>();
        if (j.contains("alwaysOnTop"))       s.alwaysOnTop        = j["alwaysOnTop"].get<bool>();
        setStyle(s); return;
    }
    if (event == "__setDragRegions") {
        auto j = json::parse(data, nullptr, false); if(!j.is_array()) return;
        std::vector<DragRegion> regions;
        for (const auto& r : j) regions.push_back({r["x"],r["y"],r["width"],r["height"]});
        setDragRegions(std::move(regions)); return;
    }
    if (event == "__windowOp") {
        auto j = json::parse(data, nullptr, false); if(j.is_discarded()) return;
        std::string op = j["op"];
        if      (op=="minimize")    minimize();
        else if (op=="maximize")    maximize();
        else if (op=="restore")     restore();
        else if (op=="close")       close();
        else if (op=="print")       print();
        else if (op=="devTools")    openDevTools(j.value("value",false));
        else if (op=="setTitle")    setTitle(j.value("value", std::string{}));
        else if (op=="alwaysOnTop") setAlwaysOnTop(j.value("value",false));
        else if (op=="fullscreen")  setFullscreen(j.value("value",false));
        else if (op=="zoom")        setZoom(j.value("value",1.0f));
        return;
    }
    if (onMessage_) onMessage_(event, data);
}

// ─── BambooClient ─────────────────────────────────────────────────────────────

void BambooClient::OnAfterCreated(CefRefPtr<CefBrowser> b) {
    CEF_REQUIRE_UI_THREAD();
    if (owner_) owner_->setCefBrowser(b);
}
bool BambooClient::DoClose(CefRefPtr<CefBrowser>) { return false; }
void BambooClient::OnBeforeClose(CefRefPtr<CefBrowser>) {
    CEF_REQUIRE_UI_THREAD();
    if (owner_) owner_->fireClose();
}
void BambooClient::OnLoadEnd(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, int http) {
    if (owner_ && frame->IsMain()) {
        owner_->fireLoad({ frame->GetURL().ToString(), http, false, {} });
        CefPostTask(TID_UI, CefCreateClosureTask([weak=std::weak_ptr(owner_)](){
            if (auto o=weak.lock()) o->injectBridgeCSS();
        }));
    }
}
void BambooClient::OnLoadError(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame,
                               ErrorCode code, const CefString& err, const CefString& url) {
    if (owner_ && frame->IsMain())
        owner_->fireLoad({ url.ToString(), static_cast<int>(code), true, err.ToString() });
}
void BambooClient::OnTitleChange(CefRefPtr<CefBrowser>, const CefString& title) {
    if (owner_) owner_->fireTitleChange(title.ToString());
}
bool BambooClient::OnConsoleMessage(CefRefPtr<CefBrowser>, cef_log_severity_t lvl,
                                    const CefString& msg, const CefString& src, int line) {
    if (!owner_) return false;
    ConsoleEvent::Level l = lvl >= LOGSEVERITY_ERROR ? ConsoleEvent::Level::Error
                          : lvl >= LOGSEVERITY_WARNING ? ConsoleEvent::Level::Warning
                          : ConsoleEvent::Level::Info;
    owner_->fireConsole({ l, msg.ToString(), src.ToString(), line });
    return false;
}
void BambooClient::OnGotFocus(CefRefPtr<CefBrowser>) { if(owner_) owner_->fireFocus(true); }
void BambooClient::OnBeforeContextMenu(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
                                       CefRefPtr<CefContextMenuParams>,
                                       CefRefPtr<CefMenuModel> model) {
    if (!owner_) return;
    auto cm = owner_->style().contextMenu;
    if (cm == ContextMenuStyle::Disabled) model->Clear();
    else if (cm == ContextMenuStyle::Custom) { model->Clear(); owner_->sendMessage("__contextMenu","null"); }
}
bool BambooClient::OnBeforeBrowse(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame,
                                  CefRefPtr<CefRequest> req, bool isRedirect, bool) {
    if (!owner_) return false;
    NavigationRequest nr{ req->GetURL().ToString(), isRedirect, frame->IsMain(), true };
    owner_->fireNavigation(nr);
    return !nr.allow;
}
void BambooClient::OnFindResult(CefRefPtr<CefBrowser>, int id, int count,
                                const CefRect&, int, bool final) {
    if (owner_ && owner_->onFind_) owner_->onFind_({ id, count, final });
}

} // namespace bamboo
