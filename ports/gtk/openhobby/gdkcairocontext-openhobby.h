#pragma once

#include "gdkconfig.h"
#include "gdkcairocontextprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_OPENHOBBY_CAIRO_CONTEXT (gdk_openhobby_cairo_context_get_type ())
#define GDK_OPENHOBBY_CAIRO_CONTEXT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_OPENHOBBY_CAIRO_CONTEXT, GdkOpenhobbyCairoContext))
#define GDK_IS_OPENHOBBY_CAIRO_CONTEXT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_OPENHOBBY_CAIRO_CONTEXT))
#define GDK_OPENHOBBY_CAIRO_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_OPENHOBBY_CAIRO_CONTEXT, GdkOpenhobbyCairoContextClass))
#define GDK_IS_OPENHOBBY_CAIRO_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_OPENHOBBY_CAIRO_CONTEXT))
#define GDK_OPENHOBBY_CAIRO_CONTEXT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_OPENHOBBY_CAIRO_CONTEXT, GdkOpenhobbyCairoContextClass))

typedef struct _GdkOpenhobbyCairoContext GdkOpenhobbyCairoContext;
typedef struct _GdkOpenhobbyCairoContextClass GdkOpenhobbyCairoContextClass;

struct _GdkOpenhobbyCairoContext
{
  GdkCairoContext parent_instance;
  cairo_surface_t *paint_surface;
};

struct _GdkOpenhobbyCairoContextClass
{
  GdkCairoContextClass parent_class;
};

GDK_AVAILABLE_IN_ALL
GType gdk_openhobby_cairo_context_get_type (void) G_GNUC_CONST;

G_END_DECLS
