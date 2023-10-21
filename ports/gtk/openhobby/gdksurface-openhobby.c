#include "config.h"

#include "gdksurface-openhobby.h"

#include "gdkopenhobbydisplayprivate.h"
#include "gdkdeviceprivate.h"
#include "gdkdevice-openhobby.h"
#include "gdkdisplay.h"
#include "gdkdragsurfaceprivate.h"
#include "gdkeventsourceprivate.h"
#include "gdkframeclockidleprivate.h"
#include "gdkframetimingsprivate.h"
#include "gdkpopupprivate.h"
#include "gdkprofilerprivate.h"
#include "gdkopenhobbydisplayprivate.h"
#include "gdkseatprivate.h"
#include "gdksurfaceprivate.h"
#include "gdktoplevelprivate.h"

#include <graphene.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

G_DEFINE_TYPE (GdkOpenhobbySurface, gdk_openhobby_surface, GDK_TYPE_SURFACE)

static void
gdk_openhobby_surface_move_resize_internal (GdkSurface *surface,
                                             gboolean    with_move,
                                             int         x,
                                             int         y,
                                             int         width,
                                             int         height);

static void
gdk_openhobby_surface_init (GdkOpenhobbySurface *impl)
{
}

static void
connect_frame_clock (GdkSurface *surface)
{
  GdkFrameClock *frame_clock = gdk_surface_get_frame_clock (surface);
}

static void
disconnect_frame_clock (GdkSurface *surface)
{
}

static void
gdk_openhobby_surface_constructed (GObject *object)
{
  GdkOpenhobbySurface *self = GDK_OPENHOBBY_SURFACE (object);
  GdkSurface *surface = GDK_SURFACE (object);
  GdkOpenhobbyDisplay *oh_display = GDK_OPENHOBBY_DISPLAY (gdk_surface_get_display (surface));
  uint32_t xnx_id = 0;

  if (!surface->parent)
    oh_display->toplevels = g_list_prepend (oh_display->toplevels, self);

  xnx_create_surface (oh_display->xnx, 1, 1, &xnx_id);
  self->id = xnx_id;
  self->buf_width = -1;
  self->buf_height = -1;

  g_hash_table_insert (oh_display->id_ht, GINT_TO_POINTER (self->id), surface);

  g_object_ref (self);

  G_OBJECT_CLASS (gdk_openhobby_surface_parent_class)->constructed (object);

  connect_frame_clock (surface);
}

static void
gdk_openhobby_surface_finalize (GObject *object)
{
  GdkOpenhobbySurface *impl;
  GdkOpenhobbyDisplay *oh_display;

  g_return_if_fail (GDK_IS_OPENHOBBY_SURFACE (object));

  impl = GDK_OPENHOBBY_SURFACE (object);

  oh_display = GDK_OPENHOBBY_DISPLAY (gdk_surface_get_display (GDK_SURFACE (impl)));

  g_hash_table_remove (oh_display->id_ht, GINT_TO_POINTER (impl->id));

  if (impl->pixels)
    g_free (impl->pixels);

  oh_display->toplevels = g_list_remove (oh_display->toplevels, impl);

  G_OBJECT_CLASS (gdk_openhobby_surface_parent_class)->finalize (object);
}

static void
_gdk_openhobby_surface_destroy (GdkSurface *surface,
                                 gboolean    foreign_destroy)
{
  GdkOpenhobbySurface *impl;
  GdkOpenhobbyDisplay *oh_display;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  impl = GDK_OPENHOBBY_SURFACE (surface);

  disconnect_frame_clock (surface);

  oh_display = GDK_OPENHOBBY_DISPLAY (gdk_surface_get_display (surface));
  g_hash_table_remove (oh_display->id_ht, GINT_TO_POINTER (impl->id));

  xnx_destroy_surface (oh_display->xnx, impl->id);
}

static void
gdk_openhobby_surface_destroy_notify (GdkSurface *surface)
{
  if (!GDK_SURFACE_DESTROYED (surface))
    _gdk_surface_destroy (surface, TRUE);

  g_object_unref (surface);
}

static void
gdk_openhobby_surface_show (GdkSurface *surface,
                             gboolean    already_mapped)
{
  GdkOpenhobbySurface *impl = GDK_OPENHOBBY_SURFACE (surface);
  impl->visible = TRUE;
}

static void
gdk_openhobby_surface_hide (GdkSurface *surface)
{
  GdkOpenhobbySurface *impl = GDK_OPENHOBBY_SURFACE (surface);
  impl->visible = FALSE;

  _gdk_surface_clear_update_area (surface);
}

static double
gdk_openhobby_surface_get_scale (GdkSurface *surface)
{
  return 1;
}

static void
gdk_openhobby_surface_move_resize_internal (GdkSurface *surface,
                                             gboolean    with_move,
                                             int         x,
                                             int         y,
                                             int         width,
                                             int         height)
{
  GdkOpenhobbySurface *impl = GDK_OPENHOBBY_SURFACE (surface);
  GdkOpenhobbyDisplay *oh_display;
  gboolean size_changed = FALSE;

  oh_display = GDK_OPENHOBBY_DISPLAY (gdk_surface_get_display (surface));

  if (with_move)
    {
      surface->x = x;
      surface->y = y;
    }

  if (width > 0 || height > 0)
    {
      if (width < 1) width = 1;
      if (height < 1) height = 1;

      if (width != surface->width || height != surface->height)
        {
          size_changed = TRUE;
          surface->width = width;
          surface->height = height;
        }
    }

  if (impl->buf_width != surface->width || impl->buf_height != surface->height)
    {
      impl->buf_width = surface->width;
      impl->buf_height = surface->height;
      if (impl->pixels)
        g_free (impl->pixels);
      impl->pixels = g_new0 (uint32_t, impl->buf_width * impl->buf_height);
    }

  xnx_set_geometry (oh_display->xnx, impl->id,
                     with_move ? surface->x : -1,
                     with_move ? surface->y : -1,
                     surface->width, surface->height);

  if (size_changed)
    {
      surface->resize_count++;
      _gdk_surface_update_size (surface);
    }
}

void
gdk_openhobby_surface_move_resize (GdkSurface *surface,
                                    int         x,
                                    int         y,
                                    int         width,
                                    int         height)
{
  gdk_openhobby_surface_move_resize_internal (surface, TRUE, x, y, width, height);
}

static void
gdk_openhobby_surface_toplevel_resize (GdkSurface *surface,
                                        int         width,
                                        int         height)
{
  gdk_openhobby_surface_move_resize_internal (surface, FALSE, 0, 0, width, height);
}

static void
gdk_openhobby_surface_move (GdkSurface *surface,
                             int         x,
                             int         y)
{
  gdk_openhobby_surface_move_resize_internal (surface, TRUE, x, y, -1, -1);
}

static void
gdk_openhobby_surface_layout_popup (GdkSurface     *surface,
                                     int             width,
                                     int             height,
                                     GdkPopupLayout *layout)
{
  GdkMonitor *monitor;
  GdkRectangle bounds;
  GdkRectangle final_rect;
  int x, y;

  monitor = gdk_surface_get_layout_monitor (surface, layout,
                                             gdk_monitor_get_geometry);
  gdk_monitor_get_geometry (monitor, &bounds);

  gdk_surface_layout_popup_helper (surface,
                                    width,
                                    height,
                                    0, 0, 0, 0,
                                    monitor,
                                    &bounds,
                                    layout,
                                    GDK_SURFACE_LAYOUT_POPUP_HELPER_DEFAULT,
                                    &final_rect);

  x = final_rect.x;
  y = final_rect.y;

  if (final_rect.width != surface->width ||
      final_rect.height != surface->height)
    {
      gdk_openhobby_surface_move_resize (surface, x, y,
                                          final_rect.width, final_rect.height);
    }
  else
    {
      gdk_openhobby_surface_move (surface, x, y);
    }
}

static gboolean
gdk_openhobby_surface_present_popup (GdkSurface     *surface,
                                      int             width,
                                      int             height,
                                      GdkPopupLayout *layout)
{
  GdkOpenhobbySurface *impl = GDK_OPENHOBBY_SURFACE (surface);

  gdk_openhobby_surface_layout_popup (surface, width, height, layout);

  if (GDK_SURFACE_IS_MAPPED (surface))
    return TRUE;

  gdk_surface_set_is_mapped (surface, TRUE);
  gdk_openhobby_surface_show (surface, FALSE);
  gdk_surface_invalidate_rect (surface, NULL);

  return GDK_SURFACE_IS_MAPPED (surface);
}

static void
gdk_openhobby_surface_get_geometry (GdkSurface *surface,
                                     int        *x,
                                     int        *y,
                                     int        *width,
                                     int        *height)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (x) *x = surface->x;
  if (y) *y = surface->y;
  if (width) *width = surface->width;
  if (height) *height = surface->height;
}

static void
gdk_openhobby_surface_get_root_coords (GdkSurface *surface,
                                        int         x,
                                        int         y,
                                        int        *root_x,
                                        int        *root_y)
{
  if (root_x) *root_x = x;
  if (root_y) *root_y = y;
}

static gboolean
gdk_openhobby_surface_get_device_state (GdkSurface      *surface,
                                         GdkDevice       *device,
                                         double          *x,
                                         double          *y,
                                         GdkModifierType *mask)
{
  g_return_val_if_fail (surface == NULL || GDK_IS_SURFACE (surface), FALSE);

  if (GDK_SURFACE_DESTROYED (surface))
    return FALSE;

  gdk_openhobby_device_query_state (device, surface, x, y, mask);

  return *x >= 0 && *y >= 0 && *x < surface->width && *y < surface->height;
}

static void
gdk_openhobby_surface_set_input_region (GdkSurface     *surface,
                                         cairo_region_t *shape_region)
{
}

static void
gdk_openhobby_surface_set_title (GdkSurface  *surface,
                                  const char *title)
{
  GdkOpenhobbySurface *impl = GDK_OPENHOBBY_SURFACE (surface);
  GdkOpenhobbyDisplay *oh_display = GDK_OPENHOBBY_DISPLAY (gdk_surface_get_display (surface));
  xnx_set_title (oh_display->xnx, impl->id, title);
}

static void
gdk_openhobby_surface_set_transient_for (GdkSurface *surface,
                                          GdkSurface *parent)
{
}

static void
gdk_openhobby_surface_set_modal_hint (GdkSurface *surface,
                                       gboolean   modal)
{
}

static void
gdk_openhobby_surface_minimize (GdkSurface *surface)
{
  if (GDK_SURFACE_DESTROYED (surface))
    return;
}

static void
gdk_openhobby_surface_unminimize (GdkSurface *surface)
{
  if (GDK_SURFACE_DESTROYED (surface))
    return;
}

static void
gdk_openhobby_surface_maximize (GdkSurface *surface)
{
  GdkOpenhobbySurface *impl;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  impl = GDK_OPENHOBBY_SURFACE (surface);

  if (impl->maximized)
    return;

  impl->maximized = TRUE;

  gdk_synthesize_surface_state (surface, 0, GDK_TOPLEVEL_STATE_MAXIMIZED);

  impl->pre_maximize_x = surface->x;
  impl->pre_maximize_y = surface->y;
  impl->pre_maximize_width = surface->width;
  impl->pre_maximize_height = surface->height;

  {
    GdkDisplay *display = gdk_surface_get_display (surface);
    GdkMonitor *monitor = gdk_display_get_monitor_at_surface (display, surface);
    if (monitor)
      {
        GdkRectangle geom;
        gdk_monitor_get_geometry (monitor, &geom);
        gdk_openhobby_surface_move_resize (surface,
                                             geom.x, geom.y,
                                             geom.width, geom.height);
      }
  }
}

static void
gdk_openhobby_surface_unmaximize (GdkSurface *surface)
{
  GdkOpenhobbySurface *impl;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  impl = GDK_OPENHOBBY_SURFACE (surface);

  if (!impl->maximized)
    return;

  impl->maximized = FALSE;

  gdk_synthesize_surface_state (surface, GDK_TOPLEVEL_STATE_MAXIMIZED, 0);

  gdk_openhobby_surface_move_resize (surface,
                                       impl->pre_maximize_x,
                                       impl->pre_maximize_y,
                                       impl->pre_maximize_width,
                                       impl->pre_maximize_height);
}

static gboolean
gdk_openhobby_surface_beep (GdkSurface *surface)
{
  return FALSE;
}

static void
gdk_openhobby_surface_request_layout (GdkSurface *surface)
{
}

static gboolean
gdk_openhobby_surface_compute_size (GdkSurface *surface)
{
  int width, height;

  if (GDK_IS_TOPLEVEL (surface))
    {
      width = surface->width;
      height = surface->height;
    }
  else
    {
      width = gdk_surface_get_width (surface);
      height = gdk_surface_get_height (surface);

      gdk_openhobby_surface_move_resize_internal (surface, FALSE, 0, 0, width, height);
    }
  return FALSE;
}

static void
gdk_openhobby_surface_class_init (GdkOpenhobbySurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceClass *impl_class = GDK_SURFACE_CLASS (klass);

  object_class->constructed = gdk_openhobby_surface_constructed;
  object_class->finalize = gdk_openhobby_surface_finalize;

  impl_class->hide = gdk_openhobby_surface_hide;
  impl_class->get_geometry = gdk_openhobby_surface_get_geometry;
  impl_class->get_root_coords = gdk_openhobby_surface_get_root_coords;
  impl_class->get_device_state = gdk_openhobby_surface_get_device_state;
  impl_class->set_input_region = gdk_openhobby_surface_set_input_region;
  impl_class->destroy = _gdk_openhobby_surface_destroy;
  impl_class->beep = gdk_openhobby_surface_beep;
  impl_class->destroy_notify = gdk_openhobby_surface_destroy_notify;
  impl_class->get_scale = gdk_openhobby_surface_get_scale;
  impl_class->request_layout = gdk_openhobby_surface_request_layout;
  impl_class->compute_size = gdk_openhobby_surface_compute_size;
}

#define LAST_PROP 1

typedef struct
{
  GdkOpenhobbySurface parent_instance;
} GdkOpenhobbyPopup;

typedef struct
{
  GdkOpenhobbySurfaceClass parent_class;
} GdkOpenhobbyPopupClass;

static void gdk_openhobby_popup_iface_init (GdkPopupInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkOpenhobbyPopup, gdk_openhobby_popup, GDK_TYPE_OPENHOBBY_SURFACE,
                          G_IMPLEMENT_INTERFACE (GDK_TYPE_POPUP,
                                                  gdk_openhobby_popup_iface_init))

static void
gdk_openhobby_popup_init (GdkOpenhobbyPopup *popup)
{
}

static void
gdk_openhobby_popup_constructed (GObject *object)
{
  GdkSurface *surface = GDK_SURFACE (object);

  gdk_surface_set_frame_clock (surface, gdk_surface_get_frame_clock (surface->parent));

  G_OBJECT_CLASS (gdk_openhobby_popup_parent_class)->constructed (object);
}

static void
gdk_openhobby_popup_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_POPUP_PROP_PARENT:
      g_value_set_object (value, surface->parent);
      break;

    case LAST_PROP + GDK_POPUP_PROP_AUTOHIDE:
      g_value_set_boolean (value, surface->autohide);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_openhobby_popup_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_POPUP_PROP_PARENT:
      surface->parent = g_value_dup_object (value);
      if (surface->parent != NULL)
        surface->parent->children = g_list_prepend (surface->parent->children, surface);
      break;

    case LAST_PROP + GDK_POPUP_PROP_AUTOHIDE:
      surface->autohide = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_openhobby_popup_class_init (GdkOpenhobbyPopupClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = gdk_openhobby_popup_constructed;
  object_class->get_property = gdk_openhobby_popup_get_property;
  object_class->set_property = gdk_openhobby_popup_set_property;

  gdk_popup_install_properties (object_class, 1);
}

static gboolean
gdk_openhobby_popup_present (GdkPopup       *popup,
                              int             width,
                              int             height,
                              GdkPopupLayout *layout)
{
  return gdk_openhobby_surface_present_popup (GDK_SURFACE (popup), width, height, layout);
}

static GdkGravity
gdk_openhobby_popup_get_surface_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.surface_anchor;
}

static GdkGravity
gdk_openhobby_popup_get_rect_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.rect_anchor;
}

static int
gdk_openhobby_popup_get_position_x (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->x;
}

static int
gdk_openhobby_popup_get_position_y (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->y;
}

static void
gdk_openhobby_popup_iface_init (GdkPopupInterface *iface)
{
  iface->present = gdk_openhobby_popup_present;
  iface->get_surface_anchor = gdk_openhobby_popup_get_surface_anchor;
  iface->get_rect_anchor = gdk_openhobby_popup_get_rect_anchor;
  iface->get_position_x = gdk_openhobby_popup_get_position_x;
  iface->get_position_y = gdk_openhobby_popup_get_position_y;
}

typedef struct
{
  GdkOpenhobbySurface parent_instance;
} GdkOpenhobbyToplevel;

typedef struct
{
  GdkOpenhobbySurfaceClass parent_class;
} GdkOpenhobbyToplevelClass;

static void gdk_openhobby_toplevel_iface_init (GdkToplevelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkOpenhobbyToplevel, gdk_openhobby_toplevel, GDK_TYPE_OPENHOBBY_SURFACE,
                          G_IMPLEMENT_INTERFACE (GDK_TYPE_TOPLEVEL,
                                                  gdk_openhobby_toplevel_iface_init))

static void
gdk_openhobby_toplevel_init (GdkOpenhobbyToplevel *toplevel)
{
}

static void
gdk_openhobby_toplevel_constructed (GObject *object)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkFrameClock *frame_clock;

  frame_clock = _gdk_frame_clock_idle_new ();
  gdk_surface_set_frame_clock (surface, frame_clock);
  g_object_unref (frame_clock);

  G_OBJECT_CLASS (gdk_openhobby_toplevel_parent_class)->constructed (object);
}

static void
gdk_openhobby_toplevel_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      gdk_openhobby_surface_set_title (surface, g_value_get_string (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      gdk_openhobby_surface_set_transient_for (surface, g_value_get_object (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      gdk_openhobby_surface_set_modal_hint (surface, g_value_get_boolean (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_ICON_LIST:
    case LAST_PROP + GDK_TOPLEVEL_PROP_DECORATED:
    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
    case LAST_PROP + GDK_TOPLEVEL_PROP_GRAVITY:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_openhobby_toplevel_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_STATE:
      g_value_set_flags (value, surface->state);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      g_value_set_static_string (value, "");
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      g_value_set_static_string (value, "");
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      g_value_set_object (value, surface->transient_for);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      g_value_set_boolean (value, surface->modal_hint);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_ICON_LIST:
      g_value_set_pointer (value, NULL);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DECORATED:
    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_CAPABILITIES:
      g_value_set_flags (value, GDK_TOPLEVEL_CAPABILITIES_MAXIMIZE |
                                GDK_TOPLEVEL_CAPABILITIES_FULLSCREEN |
                                GDK_TOPLEVEL_CAPABILITIES_MINIMIZE);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_GRAVITY:
      g_value_set_boolean (value, GDK_GRAVITY_NORTH_EAST);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_openhobby_toplevel_class_init (GdkOpenhobbyToplevelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = gdk_openhobby_toplevel_constructed;
  object_class->get_property = gdk_openhobby_toplevel_get_property;
  object_class->set_property = gdk_openhobby_toplevel_set_property;

  gdk_toplevel_install_properties (object_class, 1);
}

static void
show_surface (GdkSurface *surface)
{
  gboolean was_mapped;

  if (surface->destroyed)
    return;

  was_mapped = GDK_SURFACE_IS_MAPPED (surface);

  if (!was_mapped)
    gdk_surface_set_is_mapped (surface, TRUE);

  gdk_openhobby_surface_show (surface, FALSE);

  if (!was_mapped)
    gdk_surface_invalidate_rect (surface, NULL);
}

static void
gdk_openhobby_toplevel_present (GdkToplevel       *toplevel,
                                 GdkToplevelLayout *layout)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  int width, height;
  gboolean maximize;

  if (gdk_toplevel_layout_get_maximized (layout, &maximize))
    {
      if (maximize)
        gdk_openhobby_surface_maximize (surface);
      else
        gdk_openhobby_surface_unmaximize (surface);
    }

  width = gdk_surface_get_width (surface);
  height = gdk_surface_get_height (surface);

  if (width < 1) width = 100;
  if (height < 1) height = 100;

  gdk_surface_request_layout (surface);
  show_surface (surface);
}

static gboolean
gdk_openhobby_toplevel_minimize (GdkToplevel *toplevel)
{
  gdk_openhobby_surface_minimize (GDK_SURFACE (toplevel));
  return TRUE;
}

static void
gdk_openhobby_toplevel_focus (GdkToplevel *toplevel,
                               guint32      timestamp)
{
}

static void
gdk_openhobby_toplevel_iface_init (GdkToplevelInterface *iface)
{
  iface->present = gdk_openhobby_toplevel_present;
  iface->minimize = gdk_openhobby_toplevel_minimize;
  iface->focus = gdk_openhobby_toplevel_focus;
}

typedef struct
{
  GdkOpenhobbySurface parent_instance;
} GdkOpenhobbyDragSurface;

typedef struct
{
  GdkOpenhobbySurfaceClass parent_class;
} GdkOpenhobbyDragSurfaceClass;

static void gdk_openhobby_drag_surface_iface_init (GdkDragSurfaceInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkOpenhobbyDragSurface, gdk_openhobby_drag_surface, GDK_TYPE_OPENHOBBY_SURFACE,
                          G_IMPLEMENT_INTERFACE (GDK_TYPE_DRAG_SURFACE,
                                                  gdk_openhobby_drag_surface_iface_init))

static void
gdk_openhobby_drag_surface_init (GdkOpenhobbyDragSurface *surface)
{
}

static void
gdk_openhobby_drag_surface_constructed (GObject *object)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkFrameClock *frame_clock;

  frame_clock = _gdk_frame_clock_idle_new ();
  gdk_surface_set_frame_clock (surface, frame_clock);
  g_object_unref (frame_clock);

  G_OBJECT_CLASS (gdk_openhobby_drag_surface_parent_class)->constructed (object);
}

static void
gdk_openhobby_drag_surface_class_init (GdkOpenhobbyDragSurfaceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  object_class->constructed = gdk_openhobby_drag_surface_constructed;
}

static gboolean
gdk_openhobby_drag_surface_present (GdkDragSurface *drag_surface,
                                     int             width,
                                     int             height)
{
  GdkSurface *surface = GDK_SURFACE (drag_surface);

  gdk_openhobby_surface_toplevel_resize (surface, width, height);
  show_surface (surface);

  return TRUE;
}

static void
gdk_openhobby_drag_surface_iface_init (GdkDragSurfaceInterface *iface)
{
  iface->present = gdk_openhobby_drag_surface_present;
}
