#pragma once

#if !defined (__GDKOPENHOBBY_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/openhobby/gdkopenhobby.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _GdkOpenhobbyMonitor GdkOpenhobbyMonitor;
typedef struct _GdkOpenhobbyMonitorClass GdkOpenhobbyMonitorClass;

#define GDK_TYPE_OPENHOBBY_MONITOR              (gdk_openhobby_monitor_get_type ())
#define GDK_OPENHOBBY_MONITOR(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_OPENHOBBY_MONITOR, GdkOpenhobbyMonitor))
#define GDK_OPENHOBBY_MONITOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_OPENHOBBY_MONITOR, GdkOpenhobbyMonitorClass))
#define GDK_IS_OPENHOBBY_MONITOR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_OPENHOBBY_MONITOR))
#define GDK_IS_OPENHOBBY_MONITOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_OPENHOBBY_MONITOR))
#define GDK_OPENHOBBY_MONITOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_OPENHOBBY_MONITOR, GdkOpenhobbyMonitorClass))

GDK_AVAILABLE_IN_ALL
GType gdk_openhobby_monitor_get_type (void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GdkOpenhobbyMonitor, g_object_unref)

G_END_DECLS
