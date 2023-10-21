gdk_openhobby_sources = files([
  'gdkcairocontext-openhobby.c',
  'gdkdevice-openhobby.c',
  'gdkdisplay-openhobby.c',
  'gdkeventsource-openhobby.c',
  'gdkkeys-openhobby.c',
  'gdkmonitor-openhobby.c',
  'gdkseat-openhobby.c',
  'gdksurface-openhobby.c',
])

gdk_openhobby_public_headers = [
  'gdkopenhobbydisplay.h',
  'gdkopenhobbymonitor.h',
  'gdkopenhobbysurface.h',
]

install_headers(gdk_openhobby_public_headers, 'gdkopenhobby.h',
  subdir: 'gtk-4.0/gdk/openhobby/')

gdk_openhobby_deps = [dependency('xnx')]

libgdk_openhobby = static_library('gdk-openhobby',
  sources: [gdk_openhobby_sources, gdk_gen_headers],
  include_directories: [confinc, gdkinc],
  c_args: [
    '-DGTK_COMPILATION',
    '-DG_LOG_DOMAIN="Gdk"',
  ] + common_cflags,
  dependencies: [gdk_deps, gdk_openhobby_deps],
)
