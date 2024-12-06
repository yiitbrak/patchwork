pw_sources = [
  'main.c',
  'pw-application.c',
  'pw-window.c',
  'pw-canvas.c',
  'pw-view-controller.c',
  'pw-node.c',
  'pw-pad.c',
  'pw-dummy.c',
  'pw-pipewire.c',
  'pw-zoom-entry.c',
  'pw-misc.c',
]

libm = cc.find_library('m', required : true)
pw_deps = [
  dependency('gtk4'),
  dependency('libadwaita-1'),
  dependency('libpipewire-0.3'),
  libm,
]

pw_sources += gnome.compile_resources('pw-resources',
  'pw.gresource.xml',
  c_name: 'patchwork'
)

pw_sources += gnome.mkenums_simple('pw-types', sources : 'pw-enums.h')

pw_c_args = ['-Wno-unused-parameter',
		'-Wno-unused-variable',]

executable('patchwork', pw_sources,
  dependencies: pw_deps,
       install: true,
       c_args: pw_c_args,
       link_args: ['-rdynamic',],
)
