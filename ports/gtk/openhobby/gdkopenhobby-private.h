#pragma once

#include "gdksurface-openhobby.h"
#include "gdkopenhobbydisplayprivate.h"
#include "gdkcairocontext-openhobby.h"

#include "gdkopenhobbysurface.h"

G_BEGIN_DECLS

GdkDisplay * _gdk_openhobby_display_open (const char *display_name);
void _gdk_openhobby_display_queue_events (GdkDisplay *display);
GdkKeymap* _gdk_openhobby_display_get_keymap (GdkDisplay *display);

void _gdk_openhobby_display_process_xnx_events (GdkDisplay *display);
GSource *_gdk_openhobby_event_source_new (GdkDisplay *display);

G_END_DECLS
