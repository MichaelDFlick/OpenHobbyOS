#include "config.h"
#include <stdlib.h>

#include "gdkdevice-openhobby.h"
#include "gdkopenhobbydisplayprivate.h"
#include "gdksurface-openhobby.h"
#include "gdkseatprivate.h"

static void gdk_openhobby_device_set_surface_cursor (GdkDevice *device,
                                                      GdkSurface *surface,
                                                      GdkCursor *cursor);
static GdkSurface *gdk_openhobby_device_surface_at_position (GdkDevice       *device,
                                                               double          *win_x,
                                                               double          *win_y,
                                                               GdkModifierType *mask);

G_DEFINE_TYPE (GdkOpenhobbyDevice, gdk_openhobby_device, GDK_TYPE_DEVICE)

static void
gdk_openhobby_device_class_init (GdkOpenhobbyDeviceClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->set_surface_cursor = gdk_openhobby_device_set_surface_cursor;
  device_class->surface_at_position = gdk_openhobby_device_surface_at_position;
}

static void
gdk_openhobby_device_init (GdkOpenhobbyDevice *device)
{
  GdkDevice *dev = GDK_DEVICE (device);

  _gdk_device_add_axis (dev, GDK_AXIS_X, 0, 0, 1);
  _gdk_device_add_axis (dev, GDK_AXIS_Y, 0, 0, 1);
}

static void
gdk_openhobby_device_set_surface_cursor (GdkDevice *device,
                                          GdkSurface *surface,
                                          GdkCursor *cursor)
{
}

void
gdk_openhobby_device_query_state (GdkDevice         *device,
                                   GdkSurface        *surface,
                                   double            *win_x,
                                   double            *win_y,
                                   GdkModifierType   *mask)
{
  GdkDisplay *display = gdk_device_get_display (device);
  GdkOpenhobbyDisplay *oh_display = GDK_OPENHOBBY_DISPLAY (display);
  int origin_x, origin_y;

  if (gdk_device_get_source (device) != GDK_SOURCE_MOUSE)
    return;

  if (win_x)
    *win_x = oh_display->pointer_x;
  if (win_y)
    *win_y = oh_display->pointer_y;

  if (surface)
    {
      gdk_surface_get_origin (surface, &origin_x, &origin_y);
      if (win_x) *win_x -= origin_x;
      if (win_y) *win_y -= origin_y;
    }

  if (mask)
    *mask = 0;
}

static GdkSurface *
gdk_openhobby_device_surface_at_position (GdkDevice       *device,
                                           double          *win_x,
                                           double          *win_y,
                                           GdkModifierType *mask)
{
  GdkDisplay *display = gdk_device_get_display (device);
  GdkOpenhobbyDisplay *oh_display = GDK_OPENHOBBY_DISPLAY (display);
  GList *l;

  if (gdk_device_get_source (device) != GDK_SOURCE_MOUSE)
    return NULL;

  if (win_x)
    *win_x = oh_display->pointer_x;
  if (win_y)
    *win_y = oh_display->pointer_y;
  if (mask)
    *mask = 0;

  for (l = oh_display->toplevels; l != NULL; l = l->next)
    {
      GdkOpenhobbySurface *surface = l->data;
      int sx, sy;
      int sw, sh;

      if (!surface->visible)
        continue;

      gdk_surface_get_origin (GDK_SURFACE (surface), &sx, &sy);
      sw = gdk_surface_get_width (GDK_SURFACE (surface));
      sh = gdk_surface_get_height (GDK_SURFACE (surface));

      if (oh_display->pointer_x >= sx &&
          oh_display->pointer_x < sx + sw &&
          oh_display->pointer_y >= sy &&
          oh_display->pointer_y < sy + sh)
        {
          if (win_x) *win_x -= sx;
          if (win_y) *win_y -= sy;
          return GDK_SURFACE (surface);
        }
    }

  return NULL;
}
