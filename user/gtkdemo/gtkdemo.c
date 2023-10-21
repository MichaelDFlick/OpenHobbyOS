#include <gtk/gtk.h>
#include <cairo.h>

static gboolean
on_key_pressed (GtkEventControllerKey *controller,
                guint                   keyval,
                guint                   keycode,
                GdkModifierType         state,
                gpointer                user_data)
{
  GtkWindow *window = GTK_WINDOW (user_data);

  if (keyval == GDK_KEY_Escape || keyval == GDK_KEY_q || keyval == GDK_KEY_Q)
    {
      gtk_window_destroy (window);
      return TRUE;
    }

  return FALSE;
}

static void
on_draw (GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data)
{
  int cx = width / 2;
  int cy = height / 2;
  int r = (width < height ? width : height) * 0.4;

  cairo_set_source_rgb (cr, 0.1, 0.1, 0.2);
  cairo_paint (cr);

  cairo_set_source_rgb (cr, 0.2, 0.6, 1.0);
  cairo_arc (cr, cx, cy, r, 0, 2 * G_PI);
  cairo_fill (cr);

  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_select_font_face (cr, "sans-serif",
                          CAIRO_FONT_SLANT_NORMAL,
                          CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (cr, 24);
  cairo_text_extents_t te;
  cairo_text_extents (cr, "Hello GTK!", &te);
  cairo_move_to (cr, cx - te.width / 2 - te.x_bearing,
                     cy - te.height / 2 - te.y_bearing);
  cairo_show_text (cr, "Hello GTK!");
}

static void
activate (GtkApplication *app, gpointer user_data)
{
  GtkWidget *window;
  GtkWidget *drawing_area;
  GtkEventController *key_controller;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "GTK Demo");
  gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);

  drawing_area = gtk_drawing_area_new ();
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (drawing_area), on_draw, NULL, NULL);
  gtk_widget_set_vexpand (drawing_area, TRUE);
  gtk_widget_set_hexpand (drawing_area, TRUE);
  gtk_window_set_child (GTK_WINDOW (window), drawing_area);

  key_controller = GTK_EVENT_CONTROLLER (gtk_event_controller_key_new ());
  g_signal_connect (key_controller, "key-pressed",
                    G_CALLBACK (on_key_pressed), window);
  gtk_widget_add_controller (window, key_controller);

  gtk_window_present (GTK_WINDOW (window));
}

int
main (int argc, char *argv[])
{
  GtkApplication *app;
  int status;

  g_setenv ("GDK_BACKEND", "openhobby", TRUE);

  app = gtk_application_new ("org.openhobby.gtkdemo", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}
