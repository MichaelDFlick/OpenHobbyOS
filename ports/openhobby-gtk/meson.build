project('openhobby-gtk-server', 'c',
  version: '0.1.0',
  default_options: ['c_std=gnu11', 'buildtype=release'])

cc = meson.get_compiler('c')

pixman_dep = dependency('pixman-1')
cairo_dep = dependency('cairo')
pango_dep = dependency('pango')
pangocairo_dep = dependency('pangocairo')
fontconfig_dep = dependency('fontconfig')
freetype_dep = dependency('freetype2')
math_dep = cc.find_library('m', required: false)
glib_dep = dependency('glib-2.0')
gio_dep = dependency('gio-2.0')

xnx_inc = include_directories('../../user/lib/xnx')

executable('openhobby-gtk-server',
  'server.c',
  include_directories: [xnx_inc],
  dependencies: [
    pixman_dep, cairo_dep, pango_dep, pangocairo_dep,
    fontconfig_dep, freetype_dep, math_dep, glib_dep, gio_dep
  ],
  install: true)
