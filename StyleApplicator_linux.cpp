// bamboo/platform/StyleApplicator_linux.cpp
// Linux-specific WindowStyle application via GTK3/X11.

#include "bamboo/platform/StyleApplicator.hpp"
#include "include/cef_browser.h"

#if defined(__linux__)
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

namespace bamboo::platform {

namespace {

GtkWidget* getGtkWidget(CefRefPtr<CefBrowser> browser) {
    if (!browser) return nullptr;
    CefWindowHandle handle = browser->GetHost()->GetWindowHandle();
    // On Linux, GetWindowHandle() returns an X11 Window ID (XID).
    // We walk GTK's window list to find the matching GdkWindow.
    GList* toplevels = gtk_window_list_toplevels();
    for (GList* l = toplevels; l; l = l->next) {
        GtkWidget* w = GTK_WIDGET(l->data);
        GdkWindow* gdk = gtk_widget_get_window(w);
        if (gdk && GDK_WINDOW_XID(gdk) == handle) {
            g_list_free(toplevels);
            return w;
        }
    }
    g_list_free(toplevels);
    return nullptr;
}

::Display* getDisplay(GtkWidget* w) {
    return GDK_DISPLAY_XDISPLAY(gtk_widget_get_display(w));
}

::Window getXWindow(GtkWidget* w) {
    return GDK_WINDOW_XID(gtk_widget_get_window(w));
}

void setX11Property(GtkWidget* w, const char* name, long value) {
    Display* dpy  = getDisplay(w);
    ::Window  xwin = getXWindow(w);
    Atom prop = XInternAtom(dpy, name, False);
    Atom type = XInternAtom(dpy, "CARDINAL", False);
    XChangeProperty(dpy, xwin, prop, type, 32, PropModeReplace,
        reinterpret_cast<unsigned char*>(&value), 1);
}

} // namespace

void applyStyle(CefRefPtr<CefBrowser> browser, const WindowStyle& style) {
    GtkWidget* w = getGtkWidget(browser);
    if (!w) return;

    GtkWindow* win = GTK_WINDOW(w);

    // ── Chrome mode ───────────────────────────────────────────────────────────
    switch (style.chromeMode) {
        case ChromeMode::Full:
            gtk_window_set_decorated(win, TRUE);
            break;
        case ChromeMode::NativeTitlebar:
            gtk_window_set_decorated(win, TRUE);
            break;
        case ChromeMode::Frameless:
        case ChromeMode::CustomTitlebar:
            gtk_window_set_decorated(win, FALSE);
            break;
    }

    // ── Transparency ─────────────────────────────────────────────────────────
    if (style.transparent || style.backgroundOpacity < 1.0f) {
        GdkScreen* screen = gtk_widget_get_screen(w);
        GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
        if (visual) {
            gtk_widget_set_visual(w, visual);
            gtk_widget_set_app_paintable(w, TRUE);
        }
    }

    // ── Always on top ─────────────────────────────────────────────────────────
    gtk_window_set_keep_above(win, style.alwaysOnTop ? TRUE : FALSE);

    // ── Skip taskbar ──────────────────────────────────────────────────────────
    gtk_window_set_skip_taskbar_hint(win, style.skipTaskbar ? TRUE : FALSE);

    // ── Resize ───────────────────────────────────────────────────────────────
    setResizable(browser, style.resizable);

    // ── Shadow (compositor hint) ──────────────────────────────────────────────
    setShadow(browser, style.shadow);

    gtk_widget_queue_draw(w);
}

void setDragRegions(CefRefPtr<CefBrowser>, const std::vector<DragRegion>&) {
    // Drag regions on Linux are handled via CEF's drag handler.
    // Nothing to do at the GTK/X11 level.
}

void setCornerRadius(CefRefPtr<CefBrowser> browser, int radius) {
    GtkWidget* w = getGtkWidget(browser);
    if (!w) return;
    // Apply rounded corners via GTK CSS
    auto* provider = gtk_css_provider_new();
    auto css = std::string("window { border-radius: ") + std::to_string(radius) + "px; }";
    gtk_css_provider_load_from_data(provider, css.c_str(), -1, nullptr);
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(w),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);
}

void setMacOSVibrancy(CefRefPtr<CefBrowser>, MacOSVibrancy) {
    // No-op on Linux
}

void setWindowsMaterial(CefRefPtr<CefBrowser>, WindowsMaterial) {
    // No-op on Linux
}

void setBackgroundColor(CefRefPtr<CefBrowser> browser, Color c) {
    GtkWidget* w = getGtkWidget(browser);
    if (!w) return;
    GdkRGBA color{c.r/255.0, c.g/255.0, c.b/255.0, c.a/255.0};
    gtk_widget_override_background_color(w, GTK_STATE_FLAG_NORMAL, &color);
}

void setTransparent(CefRefPtr<CefBrowser> browser, bool transparent, float opacity) {
    GtkWidget* w = getGtkWidget(browser);
    if (!w) return;
    if (transparent) {
        GdkScreen* screen = gtk_widget_get_screen(w);
        GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
        if (visual) gtk_widget_set_visual(w, visual);
        gtk_widget_set_app_paintable(w, TRUE);
    }
    // Opacity (works on composited desktops)
    gtk_widget_set_opacity(w, static_cast<double>(opacity));
}

void setMacOSTitlebarButtonPosition(CefRefPtr<CefBrowser>, const TitlebarButtonPosition&) {
    // No-op on Linux
}

void setShadow(CefRefPtr<CefBrowser> browser, const Shadow& shadow) {
    GtkWidget* w = getGtkWidget(browser);
    if (!w) return;
    // Hint the compositor to draw/suppress shadow
    GdkWindow* gdk = gtk_widget_get_window(w);
    if (gdk) {
        // _GTK_HIDE_TITLEBAR_WHEN_MAXIMIZED is a common hint; shadow is
        // compositor-dependent. Best we can do on X11 is the _NET_WM_WINDOW_SHADOW hint.
        Display* dpy = GDK_DISPLAY_XDISPLAY(gtk_widget_get_display(w));
        ::Window xwin = GDK_WINDOW_XID(gdk);
        Atom motifAtom = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
        if (motifAtom != None) {
            struct MotifHints { unsigned long flags, functions, decorations, input_mode, status; };
            MotifHints hints{2, 0, shadow.enabled ? 1UL : 0UL, 0, 0};
            XChangeProperty(dpy, xwin, motifAtom, motifAtom, 32, PropModeReplace,
                reinterpret_cast<unsigned char*>(&hints), 5);
        }
    }
}

void setResizable(CefRefPtr<CefBrowser> browser, bool resizable) {
    GtkWidget* w = getGtkWidget(browser);
    if (!w) return;
    gtk_window_set_resizable(GTK_WINDOW(w), resizable ? TRUE : FALSE);
}

} // namespace bamboo::platform
#endif // __linux__
