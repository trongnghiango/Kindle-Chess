project(
  'kindlechess',
  'cpp',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++17',
    'warning_level=2',
    'optimization=3',
    'b_lto=true',
    'b_ndebug=if-release'
  ],
  meson_version: '>=1.1'
)

gtk_dep = dependency('gtk+-2.0')

sources = files(
  'src/chess.cpp',
  'src/config.cpp',
  'src/engine.cpp',
  'src/ui_layout.cpp',
  'src/ui_render.cpp',
  'src/app_controller.cpp',
  'src/main.cpp'
)

executable(
  'kindlechess',
  sources,
  dependencies: [gtk_dep],
  cpp_args: ['-static-libstdc++'],
  link_args: ['-static-libstdc++'],
  install: true
)
