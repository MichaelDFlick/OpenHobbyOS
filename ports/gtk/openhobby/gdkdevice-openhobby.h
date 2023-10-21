#pragma once

#include <gdk/gdkdeviceprivate.h>

G_BEGIN_DECLS

#define GDK_TYPE_OPENHOBBY_DEVICE (gdk_openhobby_device_get_type ())
#define GDK_OPENHOBBY_DEVICE(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_OPENHOBBY_DEVICE, GdkOpenhobbyDevice))
#define GDK_IS_OPENHOBBY_DEVICE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_OPENHOBBY_DEVICE))
#define GDK_OPENHOBBY_DEVICE_CLASS(c) (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_OPENHOBBY_DEVICE, GdkOpenhobbyDeviceClass))
#define GDK_IS_OPENHOBBY_DEVICE_CLASS(c) (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_OPENHOBBY_DEVICE))
#define GDK_OPENHOBBY_DEVICE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_OPENHOBBY_DEVICE, GdkOpenhobbyDeviceClass))

typedef struct _GdkOpenhobbyDevice GdkOpenhobbyDevice;
typedef struct _GdkOpenhobbyDeviceClass GdkOpenhobbyDeviceClass;

struct _GdkOpenhobbyDevice
{
  GdkDevice parent_instance;
};

struct _GdkOpenhobbyDeviceClass
{
  GdkDeviceClass parent_class;
};

G_GNUC_INTERNAL
GType gdk_openhobby_device_get_type (void) G_GNUC_CONST;

void gdk_openhobby_device_query_state (GdkDevice  *device,
                                       GdkSurface *surface,
                                       double     *win_x,
                                       double     *win_y,
                                       GdkModifierType *mask);

G_END_DECLS
