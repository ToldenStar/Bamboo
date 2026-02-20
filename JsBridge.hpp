#pragma once
// bamboo/JsBridge.hpp
// Injects the window.bamboo JavaScript API into every Chromium frame.
// Provides: send/on messaging, call/bind RPC, style control, and utility helpers.

#include <string_view>
#include "include/cef_render_process_handler.h"
#include "include/cef_v8.h"

namespace bamboo {

/**
 * @brief The full `window.bamboo` API injected into every page.
 *
 * Available from any JavaScript context:
 *
 *   // Pub/sub messaging
 *   window.bamboo.send('event', data)
 *   window.bamboo.on('event', callback)
 *   window.bamboo.off('event', callback)
 *
 *   // C++ function calls (returns Promise)
 *   const result = await window.bamboo.call('funcName', ...args)
 *
 *   // Window/style control
 *   window.bamboo.setStyle({ transparent: true, cornerRadius: 16 })
 *   window.bamboo.setDragRegions([{ x, y, width, height }])
 *   window.bamboo.minimize()
 *   window.bamboo.maximize()
 *   window.bamboo.restore()
 *   window.bamboo.close()
 *   window.bamboo.setTitle('New Title')
 *   window.bamboo.setAlwaysOnTop(true)
 *   window.bamboo.setFullscreen(true)
 *   window.bamboo.setZoom(1.5)
 *
 *   // Utilities
 *   window.bamboo.openDevTools()
 *   window.bamboo.print()
 *   window.bamboo.captureScreenshot()     // returns Promise<string> (base64 PNG)
 *   window.bamboo.version                 // "1.0.0"
 *   window.bamboo.platform                // "windows" | "macos" | "linux"
 */
inline constexpr std::string_view kBambooBridgeScript = R"js(
(function() {
  'use strict';
  if (window.bamboo) return;

  const _listeners = new Map();
  const _pending   = new Map();

  // ── Internal cefQuery wrapper ─────────────────────────────────────────────

  function _query(payload) {
    return new Promise((resolve, reject) => {
      window.cefQuery({
        request:   JSON.stringify(payload),
        onSuccess: resolve,
        onFailure: (_code, msg) => reject(new Error(msg)),
      });
    });
  }

  // ── Internal: resolve a pending call (called by C++) ──────────────────────

  function _resolveCall(id, value, error) {
    const p = _pending.get(id);
    if (!p) return;
    _pending.delete(id);
    if (error) p.reject(new Error(error));
    else       p.resolve(value);
  }

  // ── Platform detection ───────────────────────────────────────────────────

  const _platform = (() => {
    const ua = navigator.userAgent.toLowerCase();
    if (ua.includes('win'))    return 'windows';
    if (ua.includes('mac'))    return 'macos';
    return 'linux';
  })();

  // ── Public API ────────────────────────────────────────────────────────────

  window.bamboo = Object.freeze({

    // ── Meta ───────────────────────────────────────────────────────────────
    version:  '1.0.0',
    platform: _platform,

    // ── Pub/sub ────────────────────────────────────────────────────────────

    on(event, callback) {
      if (!_listeners.has(event)) _listeners.set(event, new Set());
      _listeners.get(event).add(callback);
      return () => bamboo.off(event, callback);  // returns an unsubscribe fn
    },

    off(event, callback) {
      _listeners.get(event)?.delete(callback);
    },

    send(event, data = null) {
      _query({ type: 'message', event, data }).catch(console.error);
    },

    // ── RPC ────────────────────────────────────────────────────────────────

    call(name, ...args) {
      const id = crypto.randomUUID();
      const promise = new Promise((resolve, reject) => {
        _pending.set(id, { resolve, reject });
        // Timeout after 30 seconds
        setTimeout(() => {
          if (_pending.has(id)) {
            _pending.delete(id);
            reject(new Error(`bamboo.call('${name}') timed out`));
          }
        }, 30000);
      });
      _query({ type: 'call', name, args, id }).catch(err => {
        _pending.delete(id);
        throw err;
      });
      return promise;
    },

    // ── Style / window ─────────────────────────────────────────────────────

    setStyle(styleObject) {
      return _query({ type: 'setStyle', style: styleObject });
    },

    setDragRegions(regions) {
      return _query({ type: 'setDragRegions', regions });
    },

    setTitle(title) {
      return _query({ type: 'windowOp', op: 'setTitle', value: title });
    },

    minimize()        { return _query({ type: 'windowOp', op: 'minimize' }); },
    maximize()        { return _query({ type: 'windowOp', op: 'maximize' }); },
    restore()         { return _query({ type: 'windowOp', op: 'restore'  }); },
    close()           { return _query({ type: 'windowOp', op: 'close'    }); },

    setAlwaysOnTop(v) { return _query({ type: 'windowOp', op: 'alwaysOnTop', value: v }); },
    setFullscreen(v)  { return _query({ type: 'windowOp', op: 'fullscreen',  value: v }); },
    setZoom(factor)   { return _query({ type: 'windowOp', op: 'zoom',        value: factor }); },

    openDevTools(docked = false) {
      return _query({ type: 'windowOp', op: 'devTools', value: docked });
    },

    print() {
      return _query({ type: 'windowOp', op: 'print' });
    },

    captureScreenshot() {
      return _query({ type: 'windowOp', op: 'screenshot' })
        .then(result => JSON.parse(result).data);  // base64 PNG string
    },

    // ── Internal (called by C++) ───────────────────────────────────────────

    _dispatch(event, data) {
      const listeners = _listeners.get(event);
      if (listeners) {
        for (const cb of listeners) {
          try { cb(data); } catch(e) { console.error(e); }
        }
      }
    },

    _resolveCall,
  });

  // ── CSS injection for custom chrome styles ────────────────────────────────
  // Injected by C++ via Browser::injectBridgeCSS() on each load.
  // (Placeholder — actual CSS is dynamically constructed from WindowStyle.)

})();
)js";

/**
 * @brief Renderer-process handler that installs window.bamboo on every page.
 */
class BambooJsBridge final : public CefRenderProcessHandler {
public:
    void OnContextCreated(CefRefPtr<CefBrowser>   browser,
                          CefRefPtr<CefFrame>     frame,
                          CefRefPtr<CefV8Context> context) override
    {
        frame->ExecuteJavaScript(
            std::string(kBambooBridgeScript),
            frame->GetURL(),
            0
        );
    }

    IMPLEMENT_REFCOUNTING(BambooJsBridge);
};

} // namespace bamboo
