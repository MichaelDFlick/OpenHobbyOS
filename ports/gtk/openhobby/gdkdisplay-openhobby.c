#include "config.h"

#include "gdkopenhobbydisplayprivate.h"

#include "gdkcairocontext-openhobby.h"
#include "gdkdisplay.h"
#include "gdkeventsourceprivate.h"
#include "gdkmonitor-openhobby.h"
#include "gdkseat-openhobby.h"
#include "gdkdevice-openhobby.h"
#include "gdkdeviceprivate.h"
#include "gdkprivate.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>

static void   gdk_openhobby_display_dispose            (GObject            *object);
static void   gdk_openhobby_display_finalize           (GObject            *object);

G_DEFINE_TYPE (GdkOpenhobbyDisplay, gdk_openhobby_display, GDK_TYPE_DISPLAY)

static void
gdk_openhobby_display_init (GdkOpenhobbyDisplay *display)
{
  gdk_display_set_input_shapes (GDK_DISPLAY (display), FALSE);

  display->id_ht = g_hash_table_new (NULL, NULL);

  display->monitor = g_object_new (GDK_TYPE_OPENHOBBY_MONITOR,
                                    "display", display,
                                    NULL);
  gdk_monitor_set_manufacturer (display->monitor, "OpenHobby");
  gdk_monitor_set_model (display->monitor, "XNX");
  display->display_width = 1024;
  display->display_height = 768;
  gdk_monitor_set_geometry (display->monitor, &(GdkRectangle) { 0, 0,
                              display->display_width, display->display_height });
  gdk_monitor_set_physical_size (display->monitor,
                                  display->display_width * 25.4 / 96,
                                  display->display_height * 25.4 / 96);
  gdk_monitor_set_scale_factor (display->monitor, 1);
}

static GdkDevice *
create_core_pointer (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_OPENHOBBY_DEVICE,
                        "name", "Core Pointer",
                        "source", GDK_SOURCE_MOUSE,
                        "has-cursor", TRUE,
                        "display", display,
                        NULL);
}

static GdkDevice *
create_core_keyboard (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_OPENHOBBY_DEVICE,
                        "name", "Core Keyboard",
                        "source", GDK_SOURCE_KEYBOARD,
                        "has-cursor", FALSE,
                        "display", display,
                        NULL);
}

static GdkDevice *
create_pointer (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_OPENHOBBY_DEVICE,
                        "name", "Pointer",
                        "source", GDK_SOURCE_MOUSE,
                        "has-cursor", TRUE,
                        "display", display,
                        NULL);
}

static GdkDevice *
create_keyboard (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_OPENHOBBY_DEVICE,
                        "name", "Keyboard",
                        "source", GDK_SOURCE_KEYBOARD,
                        "has-cursor", FALSE,
                        "display", display,
                        NULL);
}

static void
gdk_event_init (GdkDisplay *display)
{
  GdkOpenhobbyDisplay *oh_display;

  oh_display = GDK_OPENHOBBY_DISPLAY (display);
  oh_display->event_source = _gdk_openhobby_event_source_new (display);
}

GdkDisplay *
_gdk_openhobby_display_open (const char *display_name)
{
  GdkDisplay *display;
  GdkOpenhobbyDisplay *oh_display;
  GdkSeat *seat;
  const char *xnx_socket;
  uint32_t screen_w, screen_h;

  display = g_object_new (GDK_TYPE_OPENHOBBY_DISPLAY, NULL);
  oh_display = GDK_OPENHOBBY_DISPLAY (display);

  oh_display->core_pointer = create_core_pointer (display);
  oh_display->core_keyboard = create_core_keyboard (display);
  oh_display->pointer = create_pointer (display);
  oh_display->keyboard = create_keyboard (display);

  _gdk_device_set_associated_device (oh_display->core_pointer, oh_display->core_keyboard);
  _gdk_device_set_associated_device (oh_display->core_keyboard, oh_display->core_pointer);
  _gdk_device_set_associated_device (oh_display->pointer, oh_display->core_pointer);
  _gdk_device_set_associated_device (oh_display->keyboard, oh_display->core_keyboard);
  _gdk_device_add_physical_device (oh_display->core_pointer, oh_display->pointer);

  seat = gdk_openhobby_seat_new_for_logical_pair (oh_display->core_pointer,
                                                     oh_display->core_keyboard);

  gdk_display_add_seat (display, seat);
  gdk_openhobby_seat_add_physical_device (GDK_OPENHOBBY_SEAT (seat), oh_display->pointer);
  gdk_openhobby_seat_add_physical_device (GDK_OPENHOBBY_SEAT (seat), oh_display->keyboard);
  g_object_unref (seat);

  if (display_name == NULL)
    display_name = g_getenv ("OPENHOBBY_DISPLAY");

  xnx_socket = display_name ? display_name : "/tmp/xnx.sock";

  oh_display->xnx = xnx_connect (xnx_socket);
  if (oh_display->xnx == NULL)
    {
      g_warning ("Unable to connect to XNX compositor at %s", xnx_socket);
      g_object_unref (display);
      return NULL;
    }

  if (xnx_get_display_info (oh_display->xnx, &screen_w, &screen_h) == 0)
    {
      oh_display->display_width = screen_w;
      oh_display->display_height = screen_h;
      gdk_monitor_set_geometry (oh_display->monitor,
                                &(GdkRectangle) { 0, 0, screen_w, screen_h });
      gdk_monitor_set_physical_size (oh_display->monitor,
                                      screen_w * 25.4 / 96,
                                      screen_h * 25.4 / 96);
    }

  gdk_event_init (display);

  g_signal_emit_by_name (display, "opened");

  return display;
}

static const char *
gdk_openhobby_display_get_name (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_OPENHOBBY_DISPLAY (display), NULL);
  return "Openhobby";
}

static void
gdk_openhobby_display_beep (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_OPENHOBBY_DISPLAY (display));
}

static void
gdk_openhobby_display_sync (GdkDisplay *display)
{
}

static void
gdk_openhobby_display_flush (GdkDisplay *display)
{
}

static void
gdk_openhobby_display_dispose (GObject *object)
{
  GdkOpenhobbyDisplay *self = GDK_OPENHOBBY_DISPLAY (object);

  if (self->event_source)
    {
      g_source_destroy (self->event_source);
      g_source_unref (self->event_source);
      self->event_source = NULL;
    }
  if (self->monitors)
    {
      g_list_store_remove_all (self->monitors);
      g_clear_object (&self->monitors);
    }

  G_OBJECT_CLASS (gdk_openhobby_display_parent_class)->dispose (object);
}

static void
gdk_openhobby_display_finalize (GObject *object)
{
  GdkOpenhobbyDisplay *oh_display = GDK_OPENHOBBY_DISPLAY (object);

  if (oh_display->keymap)
    g_object_unref (oh_display->keymap);

  if (oh_display->xnx)
    {
      xnx_disconnect (oh_display->xnx);
      oh_display->xnx = NULL;
    }

  g_object_unref (oh_display->monitor);

  G_OBJECT_CLASS (gdk_openhobby_display_parent_class)->finalize (object);
}

static void
gdk_openhobby_display_notify_startup_complete (GdkDisplay  *display,
                                                const char *startup_id)
{
}

static gulong
gdk_openhobby_display_get_next_serial (GdkDisplay *display)
{
  return 0;
}

static GListModel *
gdk_openhobby_display_get_monitors (GdkDisplay *display)
{
  GdkOpenhobbyDisplay *self = GDK_OPENHOBBY_DISPLAY (display);

  if (self->monitors == NULL)
    {
      self->monitors = g_list_store_new (GDK_TYPE_MONITOR);
      g_list_store_append (self->monitors, self->monitor);
    }

  return G_LIST_MODEL (self->monitors);
}

static gboolean
gdk_openhobby_display_get_setting (GdkDisplay *display,
                                    const char *name,
                                    GValue     *value)
{
  return FALSE;
}

static void
gdk_openhobby_display_class_init (GdkOpenhobbyDisplayClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (class);

  object_class->dispose = gdk_openhobby_display_dispose;
  object_class->finalize = gdk_openhobby_display_finalize;

  display_class->toplevel_type = GDK_TYPE_OPENHOBBY_TOPLEVEL;
  display_class->popup_type = GDK_TYPE_OPENHOBBY_POPUP;
  display_class->cairo_context_type = GDK_TYPE_OPENHOBBY_CAIRO_CONTEXT;

  display_class->get_name = gdk_openhobby_display_get_name;
  display_class->beep = gdk_openhobby_display_beep;
  display_class->sync = gdk_openhobby_display_sync;
  display_class->flush = gdk_openhobby_display_flush;
  display_class->queue_events = _gdk_openhobby_display_queue_events;

  display_class->get_next_serial = gdk_openhobby_display_get_next_serial;
  display_class->notify_startup_complete = gdk_openhobby_display_notify_startup_complete;
  display_class->get_keymap = _gdk_openhobby_display_get_keymap;

  display_class->get_monitors = gdk_openhobby_display_get_monitors;
  display_class->get_setting = gdk_openhobby_display_get_setting;
}
