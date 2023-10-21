#pragma once

#include "gdkseat.h"
#include "gdkseatprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_OPENHOBBY_SEAT (gdk_openhobby_seat_get_type ())
#define GDK_OPENHOBBY_SEAT(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_OPENHOBBY_SEAT, GdkOpenhobbySeat))
#define GDK_IS_OPENHOBBY_SEAT(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_OPENHOBBY_SEAT))
#define GDK_OPENHOBBY_SEAT_CLASS(c) (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_OPENHOBBY_SEAT, GdkOpenhobbySeatClass))
#define GDK_IS_OPENHOBBY_SEAT_CLASS(c) (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_OPENHOBBY_SEAT))
#define GDK_OPENHOBBY_SEAT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_OPENHOBBY_SEAT, GdkOpenhobbySeatClass))

typedef struct _GdkOpenhobbySeat GdkOpenhobbySeat;
typedef struct _GdkOpenhobbySeatClass GdkOpenhobbySeatClass;

struct _GdkOpenhobbySeat
{
  GdkSeat parent_instance;
};

struct _GdkOpenhobbySeatClass
{
  GdkSeatClass parent_class;
};

GType gdk_openhobby_seat_get_type (void) G_GNUC_CONST;

GdkSeat *gdk_openhobby_seat_new_for_logical_pair (GdkDevice *pointer,
                                                    GdkDevice *keyboard);

void gdk_openhobby_seat_add_physical_device (GdkOpenhobbySeat *seat,
                                               GdkDevice        *device);

G_END_DECLS
