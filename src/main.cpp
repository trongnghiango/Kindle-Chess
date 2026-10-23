#include <gtk/gtk.h>
#include <cairo.h>

#include "app_controller.hpp"

using namespace kindlechess;

static AppController app;

static gboolean on_draw(GtkWidget* widget, GdkEventExpose*, gpointer) {
#if GTK_CHECK_VERSION(2, 14, 0)
    GdkWindow* win = gtk_widget_get_window(widget);
#else
    GdkWindow* win = widget->window;
#endif
    cairo_t* cr = gdk_cairo_create(win);
    app.handleDraw(widget, cr);
    cairo_destroy(cr);
    return FALSE;
}

static gboolean on_button(GtkWidget* widget, GdkEventButton* ev, gpointer) {
    if (ev->button != 1) return FALSE;
    return app.handleButtonPress(widget, static_cast<int>(ev->x), static_cast<int>(ev->y)) ? TRUE : FALSE;
}

static gboolean on_delete(GtkWidget*, GdkEvent*, gpointer) {
    app.handleExit();
    gtk_main_quit();
    return TRUE;
}

static void on_destroy(GtkWidget*, gpointer) {
    app.handleExit();
    gtk_main_quit();
}

int main(int argc, char** argv) {
    gtk_init(&argc, &argv);

    app.init(argc, argv);

    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "L:A_N:application_ID:org.garske.kindlechess_PC:T");
    gtk_window_fullscreen(GTK_WINDOW(window));
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

    GtkWidget* area = gtk_drawing_area_new();
    gtk_widget_add_events(area, GDK_BUTTON_PRESS_MASK);
    gtk_container_add(GTK_CONTAINER(window), area);

    app.setupGtk(window, area);

    g_signal_connect(G_OBJECT(area), "expose-event", G_CALLBACK(on_draw), nullptr);
    g_signal_connect(G_OBJECT(area), "button-press-event", G_CALLBACK(on_button), nullptr);
    g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(on_delete), nullptr);
    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(on_destroy), nullptr);

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}
