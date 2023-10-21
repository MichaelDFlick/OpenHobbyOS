#pragma once

#include <gdk/gdksurfaceprivate.h>
#include "gdkopenhobbysurface.h"

G_BEGIN_DECLS

#define GDK_TYPE_OPENHOBBY_TOPLEVEL (gdk_openhobby_toplevel_get_type ())
#define GDK_TYPE_OPENHOBBY_POPUP (gdk_openhobby_popup_get_type ())
#define GDK_TYPE_OPENHOBBY_DRAG_SURFACE (gdk_openhobby_drag_surface_get_type ())

GType gdk_openhobby_toplevel_get_type (void) G_GNUC_CONST;
GType gdk_openhobby_popup_get_type (void) G_GNUC_CONST;
GType gdk_openhobby_drag_surface_get_type (void) G_GNUC_CONST;

struct _GdkOpenhobbySurface
{
  GdkSurface parent_instance;

  int id;
  uint32_t *pixels;
  int buf_width;
  int buf_height;

  gboolean visible;
  gboolean maximized;

  int pre_maximize_x;
  int pre_maximize_y;
  int pre_maximize_width;
  int pre_maximize_height;
};

struct _GdkOpenhobbySurfaceClass
{
  GdkSurfaceClass parent_class;
};

G_END_DECLS
