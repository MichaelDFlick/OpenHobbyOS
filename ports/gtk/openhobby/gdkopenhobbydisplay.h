#pragma once

#if !defined (__GDKOPENHOBBY_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/openhobby/gdkopenhobby.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

#ifdef GTK_COMPILATION
typedef struct _GdkOpenhobbyDisplay GdkOpenhobbyDisplay;
#else
typedef GdkDisplay GdkOpenhobbyDisplay;
#endif
typedef struct _GdkOpenhobbyDisplayClass GdkOpenhobbyDisplayClass;

#define GDK_TYPE_OPENHOBBY_DISPLAY              (gdk_openhobby_display_get_type ())
#define GDK_OPENHOBBY_DISPLAY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_OPENHOBBY_DISPLAY, GdkOpenhobbyDisplay))
#define GDK_OPENHOBBY_DISPLAY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_OPENHOBBY_DISPLAY, GdkOpenhobbyDisplayClass))
#define GDK_IS_OPENHOBBY_DISPLAY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_OPENHOBBY_DISPLAY))
#define GDK_IS_OPENHOBBY_DISPLAY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_OPENHOBBY_DISPLAY))
#define GDK_OPENHOBBY_DISPLAY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_OPENHOBBY_DISPLAY, GdkOpenhobbyDisplayClass))

GDK_AVAILABLE_IN_ALL
GType gdk_openhobby_display_get_type (void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GdkOpenhobbyDisplay, g_object_unref)

G_END_DECLS
