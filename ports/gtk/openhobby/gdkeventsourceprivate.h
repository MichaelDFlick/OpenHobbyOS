#pragma once

#include "gdkopenhobby-private.h"

G_BEGIN_DECLS

typedef struct _GdkEventSource GdkEventSource;

G_GNUC_INTERNAL
GSource * _gdk_openhobby_event_source_new            (GdkDisplay *display);

G_END_DECLS
