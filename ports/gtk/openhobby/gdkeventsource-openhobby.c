#include "config.h"

#include "gdkeventsourceprivate.h"

#include "gdkeventsprivate.h"
#include "gdkframeclockprivate.h"
#include "gdkdisplayprivate.h"
#include "gdksurfaceprivate.h"

#include "gdkopenhobbydisplayprivate.h"
#include "xnx/xnx.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static gboolean gdk_event_source_prepare  (GSource     *source,
                                            int         *timeout);
static gboolean gdk_event_source_check    (GSource     *source);
static gboolean gdk_event_source_dispatch (GSource     *source,
                                            GSourceFunc  callback,
                                            gpointer     user_data);
static void     gdk_event_source_finalize (GSource     *source);

struct _GdkEventSource
{
  GSource source;

  GdkDisplay *display;
  GPollFD event_poll_fd;
};

static GSourceFuncs event_funcs = {
  gdk_event_source_prepare,
  gdk_event_source_check,
  gdk_event_source_dispatch,
  gdk_event_source_finalize
};

static GList *event_sources = NULL;

static gboolean
gdk_event_source_prepare (GSource *source,
                           int     *timeout)
{
  GdkDisplay *display = ((GdkEventSource*) source)->display;
  gboolean retval;

  *timeout = -1;

  retval = (_gdk_event_queue_find_first (display) != NULL);

  return retval;
}

static gboolean
gdk_event_source_check (GSource *source)
{
  GdkEventSource *event_source = (GdkEventSource*) source;
  gboolean retval;

  if (event_source->display->event_pause_count > 0 ||
      event_source->event_poll_fd.revents & G_IO_IN)
    retval = (_gdk_event_queue_find_first (event_source->display) != NULL);
  else
    retval = FALSE;

  return retval;
}

static void
handle_focus_change (GdkEvent *event)
{
  GdkEvent *focus_event;
  gboolean focus_in = (gdk_event_get_event_type (event) == GDK_ENTER_NOTIFY);
  GdkNotifyType detail;

  if (gdk_event_get_event_type (event) != GDK_ENTER_NOTIFY &&
      gdk_event_get_event_type (event) != GDK_LEAVE_NOTIFY)
    return;

  detail = gdk_crossing_event_get_detail (event);

  if (detail == GDK_NOTIFY_INFERIOR)
    return;

  if (!gdk_crossing_event_get_focus (event))
    return;

  focus_event = gdk_focus_event_new (gdk_event_get_surface (event),
                                     gdk_event_get_device (event),
                                     focus_in);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_display_put_event (gdk_event_get_display (event), focus_event);
G_GNUC_END_IGNORE_DEPRECATIONS
  gdk_event_unref (focus_event);
}

static GdkModifierType
xnx_buttons_to_gdk (uint8_t buttons)
{
  GdkModifierType state = 0;
  if (buttons & 1) state |= GDK_BUTTON1_MASK;
  if (buttons & 2) state |= GDK_BUTTON3_MASK;
  if (buttons & 4) state |= GDK_BUTTON2_MASK;
  return state;
}

void
_gdk_openhobby_display_process_xnx_events (GdkDisplay *display)
{
  GdkOpenhobbyDisplay *oh_display = GDK_OPENHOBBY_DISPLAY (display);
  struct xnx_event ev;
  GList *node;
  GdkEvent *event = NULL;
  GdkModifierType state;
  GdkSurface *surface;
  uint32_t timestamp;
  static uint32_t event_counter = 1;

  while (xnx_poll_event (oh_display->xnx, &ev) == 0)
    {
      GdkModifierType mask = 0;
      double win_x = 0, win_y = 0;

      timestamp = event_counter++;

      switch (ev.type)
        {
        case XNX_EVENT_KEY:
          surface = g_hash_table_lookup (oh_display->id_ht,
                                         GINT_TO_POINTER (ev.data.key.surface_id));
          if (surface)
            {
              GdkTranslatedKey translated;
              memset (&translated, 0, sizeof (translated));
              translated.keyval = ev.data.key.keycode;

              event = gdk_key_event_new (ev.data.key.pressed
                                           ? GDK_KEY_PRESS
                                           : GDK_KEY_RELEASE,
                                         surface,
                                         oh_display->core_keyboard,
                                         timestamp,
                                         ev.data.key.keycode,
                                         0,
                                         FALSE,
                                         &translated,
                                         &translated,
                                         NULL);
              node = _gdk_event_queue_append (display, event);
              _gdk_windowing_got_event (display, node, event, 0);
            }
          break;

        case XNX_EVENT_POINTER:
          state = xnx_buttons_to_gdk (ev.data.pointer.buttons);

          surface = g_hash_table_lookup (oh_display->id_ht,
                                         GINT_TO_POINTER (ev.data.pointer.surface_id));
          if (surface)
            {
              oh_display->pointer_x = ev.data.pointer.x;
              oh_display->pointer_y = ev.data.pointer.y;

              win_x = ev.data.pointer.x;
              win_y = ev.data.pointer.y;

              {
                int origin_x = 0, origin_y = 0;
                gdk_surface_get_origin (surface, &origin_x, &origin_y);
                win_x -= origin_x;
                win_y -= origin_y;
              }

              uint8_t changed = oh_display->last_buttons ^ ev.data.pointer.buttons;

              if (changed & 1)
                {
                  event = gdk_button_event_new (
                    ev.data.pointer.buttons & 1 ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE,
                    surface,
                    oh_display->core_pointer,
                    NULL,
                    timestamp,
                    state,
                    1,
                    win_x,
                    win_y,
                    NULL);

                  node = _gdk_event_queue_append (display, event);
                  _gdk_windowing_got_event (display, node, event, 0);
                }
              if (changed & 2)
                {
                  event = gdk_button_event_new (
                    ev.data.pointer.buttons & 2 ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE,
                    surface,
                    oh_display->core_pointer,
                    NULL,
                    timestamp,
                    state,
                    3,
                    win_x,
                    win_y,
                    NULL);

                  node = _gdk_event_queue_append (display, event);
                  _gdk_windowing_got_event (display, node, event, 0);
                }
              if (changed & 4)
                {
                  event = gdk_button_event_new (
                    ev.data.pointer.buttons & 4 ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE,
                    surface,
                    oh_display->core_pointer,
                    NULL,
                    timestamp,
                    state,
                    2,
                    win_x,
                    win_y,
                    NULL);

                  node = _gdk_event_queue_append (display, event);
                  _gdk_windowing_got_event (display, node, event, 0);
                }

              if (oh_display->last_buttons == ev.data.pointer.buttons ||
                  changed == 0)
                {
                  event = gdk_motion_event_new (surface,
                                                  oh_display->core_pointer,
                                                  NULL,
                                                  timestamp,
                                                  state,
                                                  win_x,
                                                  win_y,
                                                  NULL);
                  node = _gdk_event_queue_append (display, event);
                  _gdk_windowing_got_event (display, node, event, 0);
                }

              oh_display->last_buttons = ev.data.pointer.buttons;
            }
          break;

        default:
          break;
        }
    }

  if (event)
    _gdk_windowing_got_event (display, NULL, NULL, 0);
}

void
_gdk_openhobby_display_queue_events (GdkDisplay *display)
{
  GdkOpenhobbyDisplay *oh_display = GDK_OPENHOBBY_DISPLAY (display);

  g_assert (oh_display != NULL);
  g_assert (oh_display->xnx != NULL);

  _gdk_openhobby_display_process_xnx_events (display);
}

static gboolean
gdk_event_source_dispatch (GSource     *source,
                            GSourceFunc  callback,
                            gpointer     user_data)
{
  GdkDisplay *display = ((GdkEventSource*) source)->display;
  GdkEvent *event;

  event = gdk_display_get_event (display);

  if (event)
    {
      _gdk_event_emit (event);

      gdk_event_unref (event);
    }

  return TRUE;
}

static void
gdk_event_source_finalize (GSource *source)
{
  GdkEventSource *event_source = (GdkEventSource *)source;

  event_sources = g_list_remove (event_sources, event_source);
}

GSource *
_gdk_openhobby_event_source_new (GdkDisplay *display)
{
  GSource *source;
  GdkEventSource *event_source;
  GdkOpenhobbyDisplay *oh_display = GDK_OPENHOBBY_DISPLAY (display);
  char *name;

  source = g_source_new (&event_funcs, sizeof (GdkEventSource));
  name = g_strdup_printf ("GDK Openhobby Event source (%s)",
                           gdk_display_get_name (display));
  g_source_set_name (source, name);
  g_free (name);
  event_source = (GdkEventSource *) source;
  event_source->display = display;

  event_source->event_poll_fd.fd = xnx_get_fd (oh_display->xnx);
  event_source->event_poll_fd.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
  g_source_add_poll (source, &event_source->event_poll_fd);

  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  event_sources = g_list_prepend (event_sources, source);

  return source;
}
