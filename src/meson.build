pw_sources = [
  'main.c',
  'pw-application.c',
  'pw-window.c',
  'pw-canvas.c',
  'pw-view-controller.c',
  'pw-node.c',
  'pw-pad.c',
  'pw-dummy.c',
]

pw_deps = [
  dependency('gtk4'),
  dependency('libadwaita-1'),
]

pw_sources += gnome.compile_resources('pw-resources',
  'pw.gresource.xml',
  c_name: 'patchwork'
)

pw_enums = gnome.mkenums_simple('pw-types', sources : 'pw-enums.h')

pw_c_args = ['-Wno-unused-parameter',
		'-Wno-unused-variable',]

executable('patchwork', pw_sources, pw_enums,
  dependencies: pw_deps,
       install: true,
       c_args: pw_c_args,
       link_args: ['-rdynamic',],
)
