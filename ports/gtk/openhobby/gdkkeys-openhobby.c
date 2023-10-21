#include "config.h"
#include "gdkopenhobbydisplayprivate.h"
#include "gdkkeysprivate.h"
#include "gdkkeysyms.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _GdkOpenhobbyKeymap   GdkOpenhobbyKeymap;
typedef struct _GdkKeymapClass GdkOpenhobbyKeymapClass;

#define GDK_TYPE_OPENHOBBY_KEYMAP          (gdk_openhobby_keymap_get_type ())
#define GDK_OPENHOBBY_KEYMAP(object)       (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_OPENHOBBY_KEYMAP, GdkOpenhobbyKeymap))
#define GDK_IS_OPENHOBBY_KEYMAP(object)    (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_OPENHOBBY_KEYMAP))

static GType gdk_openhobby_keymap_get_type (void);

struct _GdkOpenhobbyKeymap
{
  GdkKeymap parent_instance;
};

struct _GdkOpenhobbyKeymapClass
{
  GdkKeymapClass parent_class;
};

G_DEFINE_TYPE (GdkOpenhobbyKeymap, gdk_openhobby_keymap, GDK_TYPE_KEYMAP)

static void
gdk_openhobby_keymap_init (GdkOpenhobbyKeymap *keymap)
{
}

static void
gdk_openhobby_keymap_finalize (GObject *object)
{
  G_OBJECT_CLASS (gdk_openhobby_keymap_parent_class)->finalize (object);
}

static void
gdk_openhobby_keymap_class_init (GdkOpenhobbyKeymapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gdk_openhobby_keymap_finalize;
}

GdkKeymap *
_gdk_openhobby_display_get_keymap (GdkDisplay *display)
{
  GdkOpenhobbyDisplay *oh_display = GDK_OPENHOBBY_DISPLAY (display);

  if (oh_display->keymap == NULL)
    {
      oh_display->keymap = g_object_new (GDK_TYPE_OPENHOBBY_KEYMAP, NULL);
    }

  return oh_display->keymap;
}
