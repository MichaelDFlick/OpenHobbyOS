#include "gdkopenhobbydisplayprivate.h"
#include "gdkseat-openhobby.h"
#include "gdksurface-openhobby.h"
#include "gdkdevicetoolprivate.h"

typedef struct _GdkOpenhobbySeatPrivate GdkOpenhobbySeatPrivate;

struct _GdkOpenhobbySeatPrivate
{
  GdkDevice *logical_pointer;
  GdkDevice *logical_keyboard;
  GList *physical_pointers;
  GList *physical_keyboards;
  GdkSeatCapabilities capabilities;
  GPtrArray *tools;
};

G_DEFINE_TYPE_WITH_PRIVATE (GdkOpenhobbySeat, gdk_openhobby_seat, GDK_TYPE_SEAT)

static void
gdk_openhobby_seat_dispose (GObject *object)
{
  GdkOpenhobbySeat *seat = GDK_OPENHOBBY_SEAT (object);
  GdkOpenhobbySeatPrivate *priv = gdk_openhobby_seat_get_instance_private (seat);
  GList *l;

  if (priv->logical_pointer)
    {
      gdk_seat_device_removed (GDK_SEAT (seat), priv->logical_pointer);
      g_clear_object (&priv->logical_pointer);
    }
  if (priv->logical_keyboard)
    {
      gdk_seat_device_removed (GDK_SEAT (seat), priv->logical_keyboard);
      g_clear_object (&priv->logical_keyboard);
    }

  for (l = priv->physical_pointers; l; l = l->next)
    {
      gdk_seat_device_removed (GDK_SEAT (seat), l->data);
      g_object_unref (l->data);
    }
  for (l = priv->physical_keyboards; l; l = l->next)
    {
      gdk_seat_device_removed (GDK_SEAT (seat), l->data);
      g_object_unref (l->data);
    }

  g_clear_pointer (&priv->tools, g_ptr_array_unref);
  g_list_free (priv->physical_pointers);
  g_list_free (priv->physical_keyboards);

  G_OBJECT_CLASS (gdk_openhobby_seat_parent_class)->dispose (object);
}

static GdkSeatCapabilities
gdk_openhobby_seat_get_capabilities (GdkSeat *seat)
{
  GdkOpenhobbySeatPrivate *priv = gdk_openhobby_seat_get_instance_private (GDK_OPENHOBBY_SEAT (seat));
  return priv->capabilities;
}

static GdkGrabStatus
gdk_openhobby_seat_grab (GdkSeat    *seat,
                          GdkSurface *surface)
{
  return GDK_GRAB_SUCCESS;
}

static void
gdk_openhobby_seat_ungrab (GdkSeat    *seat,
                            GdkSurface *surface)
{
}

static GdkDevice *
gdk_openhobby_seat_get_logical_device (GdkSeat             *seat,
                                        GdkSeatCapabilities  capability)
{
  GdkOpenhobbySeatPrivate *priv = gdk_openhobby_seat_get_instance_private (GDK_OPENHOBBY_SEAT (seat));

  switch ((guint) capability)
    {
    case GDK_SEAT_CAPABILITY_POINTER:
    case GDK_SEAT_CAPABILITY_TOUCH:
      return priv->logical_pointer;
    case GDK_SEAT_CAPABILITY_KEYBOARD:
      return priv->logical_keyboard;
    default:
      break;
    }

  return NULL;
}

static GdkSeatCapabilities
device_get_capability (GdkDevice *device)
{
  GdkInputSource source = gdk_device_get_source (device);

  switch (source)
    {
    case GDK_SOURCE_KEYBOARD:
      return GDK_SEAT_CAPABILITY_KEYBOARD;
    case GDK_SOURCE_MOUSE:
    case GDK_SOURCE_TOUCHPAD:
    case GDK_SOURCE_TRACKPOINT:
      return GDK_SEAT_CAPABILITY_POINTER;
    case GDK_SOURCE_TOUCHSCREEN:
      return GDK_SEAT_CAPABILITY_TOUCH;
    case GDK_SOURCE_PEN:
      return GDK_SEAT_CAPABILITY_TABLET_STYLUS;
    case GDK_SOURCE_TABLET_PAD:
      return GDK_SEAT_CAPABILITY_TABLET_PAD;
    default:
      return GDK_SEAT_CAPABILITY_NONE;
    }
}

static GList *
gdk_openhobby_seat_get_devices (GdkSeat             *seat,
                                 GdkSeatCapabilities  capabilities)
{
  GdkOpenhobbySeatPrivate *priv = gdk_openhobby_seat_get_instance_private (GDK_OPENHOBBY_SEAT (seat));
  GList *devices = NULL;
  GList *l;

  if (capabilities & GDK_SEAT_CAPABILITY_ALL_POINTING)
    {
      for (l = priv->physical_pointers; l; l = l->next)
        {
          if (device_get_capability (l->data) & capabilities)
            devices = g_list_prepend (devices, l->data);
        }
    }

  if (capabilities & (GDK_SEAT_CAPABILITY_KEYBOARD | GDK_SEAT_CAPABILITY_TABLET_PAD))
    {
      for (l = priv->physical_keyboards; l; l = l->next)
        {
          if (device_get_capability (l->data) & capabilities)
            devices = g_list_prepend (devices, l->data);
        }
    }

  return devices;
}

static void
gdk_openhobby_seat_class_init (GdkOpenhobbySeatClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSeatClass *seat_class = GDK_SEAT_CLASS (klass);

  object_class->dispose = gdk_openhobby_seat_dispose;
  seat_class->get_capabilities = gdk_openhobby_seat_get_capabilities;
  seat_class->grab = gdk_openhobby_seat_grab;
  seat_class->ungrab = gdk_openhobby_seat_ungrab;
  seat_class->get_logical_device = gdk_openhobby_seat_get_logical_device;
  seat_class->get_devices = gdk_openhobby_seat_get_devices;
}

static void
gdk_openhobby_seat_init (GdkOpenhobbySeat *seat)
{
}

GdkSeat *
gdk_openhobby_seat_new_for_logical_pair (GdkDevice *pointer,
                                          GdkDevice *keyboard)
{
  GdkOpenhobbySeatPrivate *priv;
  GdkDisplay *display = gdk_device_get_display (pointer);
  GdkSeat *seat;

  seat = g_object_new (GDK_TYPE_OPENHOBBY_SEAT,
                        "display", display,
                        NULL);

  priv = gdk_openhobby_seat_get_instance_private (GDK_OPENHOBBY_SEAT (seat));
  priv->logical_pointer = g_object_ref (pointer);
  priv->logical_keyboard = g_object_ref (keyboard);

  gdk_seat_device_added (seat, priv->logical_pointer);
  gdk_seat_device_added (seat, priv->logical_keyboard);

  return seat;
}

void
gdk_openhobby_seat_add_physical_device (GdkOpenhobbySeat *seat,
                                         GdkDevice        *device)
{
  GdkOpenhobbySeatPrivate *priv = gdk_openhobby_seat_get_instance_private (seat);
  GdkSeatCapabilities capability = device_get_capability (device);

  if (capability & GDK_SEAT_CAPABILITY_ALL_POINTING)
    priv->physical_pointers = g_list_prepend (priv->physical_pointers, g_object_ref (device));
  else if (capability & (GDK_SEAT_CAPABILITY_KEYBOARD | GDK_SEAT_CAPABILITY_TABLET_PAD))
    priv->physical_keyboards = g_list_prepend (priv->physical_keyboards, g_object_ref (device));
  else
    return;

  priv->capabilities |= capability;
  gdk_seat_device_added (GDK_SEAT (seat), device);
}
