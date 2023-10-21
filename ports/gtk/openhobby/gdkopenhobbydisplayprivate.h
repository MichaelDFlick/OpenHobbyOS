#pragma once

#include <glib.h>
#include <gio/gio.h>

#include <gdk/gdkdisplayprivate.h>
#include <gdk/gdkmonitorprivate.h>
#include <gdk/gdkseatprivate.h>

#include "gdkopenhobby.h"

#include "gdksurface-openhobby.h"
#include "gdkdevice-openhobby.h"
#include "gdkseat-openhobby.h"
#include "gdkmonitor-openhobby.h"
#include "gdkcairocontext-openhobby.h"

#include <gdk/gdk.h>
#include <gdk/gdkdeviceprivate.h>

#include <xnx/xnx.h>

G_BEGIN_DECLS

struct _GdkOpenhobbyDisplay
{
  GdkDisplay parent_instance;

  xnx_conn_t *xnx;
  int event_fd;

  GHashTable *id_ht;
  GList *toplevels;

  GdkDevice *core_pointer;
  GdkDevice *core_keyboard;
  GdkDevice *pointer;
  GdkDevice *keyboard;

  GListStore *monitors;
  GdkMonitor *monitor;

  guint display_width;
  guint display_height;

  /* Mapping of XNX pointer button state (bitmask) to last sent GDK button events */
  guint8 last_buttons;
  int pointer_x;
  int pointer_y;

  GdkKeymap *keymap;
  GSource *event_source;
  GListStore *monitors_store;
};

struct _GdkOpenhobbyDisplayClass
{
  GdkDisplayClass parent_class;
};

GdkDisplay *_gdk_openhobby_display_open (const char *display_name);
void _gdk_openhobby_display_queue_events (GdkDisplay *display);
GdkKeymap *_gdk_openhobby_display_get_keymap (GdkDisplay *display);

void _gdk_openhobby_display_process_xnx_events (GdkDisplay *display);

GSource *_gdk_openhobby_event_source_new (GdkDisplay *display);

G_END_DECLS
