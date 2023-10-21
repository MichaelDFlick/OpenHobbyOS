#pragma once

#if !defined (__GDKOPENHOBBY_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/openhobby/gdkopenhobby.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

#ifdef GTK_COMPILATION
typedef struct _GdkOpenhobbySurface GdkOpenhobbySurface;
#else
typedef GdkSurface GdkOpenhobbySurface;
#endif
typedef struct _GdkOpenhobbySurfaceClass GdkOpenhobbySurfaceClass;

#define GDK_TYPE_OPENHOBBY_SURFACE              (gdk_openhobby_surface_get_type ())
#define GDK_OPENHOBBY_SURFACE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_OPENHOBBY_SURFACE, GdkOpenhobbySurface))
#define GDK_OPENHOBBY_SURFACE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_OPENHOBBY_SURFACE, GdkOpenhobbySurfaceClass))
#define GDK_IS_OPENHOBBY_SURFACE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_OPENHOBBY_SURFACE))
#define GDK_IS_OPENHOBBY_SURFACE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_OPENHOBBY_SURFACE))
#define GDK_OPENHOBBY_SURFACE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_OPENHOBBY_SURFACE, GdkOpenhobbySurfaceClass))

GDK_AVAILABLE_IN_ALL
GType gdk_openhobby_surface_get_type (void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GdkOpenhobbySurface, g_object_unref)

G_END_DECLS
