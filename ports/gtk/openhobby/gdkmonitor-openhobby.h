#pragma once

#include <glib.h>
#include <gio/gio.h>

#include "gdkmonitorprivate.h"
#include "gdkopenhobbymonitor.h"

struct _GdkOpenhobbyMonitor
{
  GdkMonitor parent;
};

struct _GdkOpenhobbyMonitorClass
{
  GdkMonitorClass parent_class;
};
