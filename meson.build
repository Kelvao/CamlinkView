project('camlink-view', 'c',
  version : '0.1.0',
  default_options : ['warning_level=2', 'c_std=c11']
)

sdl2_dep     = dependency('sdl2')
pipewire_dep = dependency('libpipewire-0.3')
libspa_dep   = dependency('libspa-0.2')

# Nota: font.h e menu_font.h são headers-only (stb-style com *_IMPLEMENTATION).
# Não têm .c correspondente — a implementação é instanciada em overlay.c e menu.c.
sources = files(
  'src/main.c',
  'src/clock.c',
  'src/detect.c',
  'src/stats.c',
  'src/video.c',
  'src/audio.c',
  'src/renderer.c',
  'src/controls.c',
  'src/menu.c',
  'src/overlay.c',
)

executable('camlink-view',
  sources,
  dependencies : [sdl2_dep, pipewire_dep, libspa_dep],
  install : true
)
