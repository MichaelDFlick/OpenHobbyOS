#include "config.h"

#include "gdkconfig.h"

#include "gdkcairocontext-openhobby.h"
#include "gdkopenhobbydisplayprivate.h"
#include "gdksurface-openhobby.h"

G_DEFINE_TYPE (GdkOpenhobbyCairoContext, gdk_openhobby_cairo_context, GDK_TYPE_CAIRO_CONTEXT)

static void
gdk_openhobby_cairo_context_dispose (GObject *object)
{
  G_OBJECT_CLASS (gdk_openhobby_cairo_context_parent_class)->dispose (object);
}

static void
gdk_openhobby_cairo_context_begin_frame (GdkDrawContext  *draw_context,
                                         gpointer         context_data,
                                         GdkMemoryDepth   depth,
                                         cairo_region_t  *region,
                                         GdkColorState  **out_color_state,
                                         GdkMemoryDepth  *out_depth)
{
  GdkOpenhobbyCairoContext *self = GDK_OPENHOBBY_CAIRO_CONTEXT (draw_context);
  cairo_t *cr;
  cairo_region_t *repaint_region;
  guint width, height;

  gdk_draw_context_get_buffer_size (draw_context, &width, &height);
  self->paint_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  repaint_region = cairo_region_create_rectangle (&(cairo_rectangle_int_t) { 0, 0, width, height });
  cairo_region_union (region, repaint_region);
  cairo_region_destroy (repaint_region);

  cr = cairo_create (self->paint_surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_fill (cr);
  cairo_destroy (cr);

  *out_color_state = GDK_COLOR_STATE_SRGB;
  *out_depth = gdk_color_state_get_depth (GDK_COLOR_STATE_SRGB);
}

static void
gdk_openhobby_cairo_context_end_frame (GdkDrawContext *draw_context,
                                       gpointer        context_data,
                                       cairo_region_t *painted)
{
  GdkOpenhobbyCairoContext *self = GDK_OPENHOBBY_CAIRO_CONTEXT (draw_context);
  GdkDisplay *display = gdk_draw_context_get_display (draw_context);
  GdkSurface *surface = gdk_draw_context_get_surface (draw_context);
  GdkOpenhobbySurface *oh_surface = GDK_OPENHOBBY_SURFACE (surface);
  GdkOpenhobbyDisplay *oh_display = GDK_OPENHOBBY_DISPLAY (display);
  int stride, w, h;
  const unsigned char *data;

  stride = cairo_image_surface_get_stride (self->paint_surface);
  w = cairo_image_surface_get_width (self->paint_surface);
  h = cairo_image_surface_get_height (self->paint_surface);
  data = cairo_image_surface_get_data (self->paint_surface);

  xnx_write_buffer (oh_display->xnx, oh_surface->id, 0, 0, w, h, (const uint32_t *)data);
  xnx_commit (oh_display->xnx, oh_surface->id);

  cairo_surface_destroy (self->paint_surface);
  self->paint_surface = NULL;
}

static void
gdk_openhobby_cairo_context_surface_resized (GdkDrawContext *draw_context)
{
}

static cairo_t *
gdk_openhobby_cairo_context_cairo_create (GdkCairoContext *context)
{
  GdkOpenhobbyCairoContext *self = GDK_OPENHOBBY_CAIRO_CONTEXT (context);

  return cairo_create (self->paint_surface);
}

static void
gdk_openhobby_cairo_context_class_init (GdkOpenhobbyCairoContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);
  GdkCairoContextClass *cairo_context_class = GDK_CAIRO_CONTEXT_CLASS (klass);

  gobject_class->dispose = gdk_openhobby_cairo_context_dispose;

  draw_context_class->begin_frame = gdk_openhobby_cairo_context_begin_frame;
  draw_context_class->end_frame = gdk_openhobby_cairo_context_end_frame;
  draw_context_class->surface_resized = gdk_openhobby_cairo_context_surface_resized;

  cairo_context_class->cairo_create = gdk_openhobby_cairo_context_cairo_create;
}

static void
gdk_openhobby_cairo_context_init (GdkOpenhobbyCairoContext *self)
{
}
