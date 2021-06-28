project(
  'clox',
  ['c', 'cpp'],
  default_options : [
    'warning_level=3',
    'default_library=static',
  ]
)

add_project_arguments(
  '-Wuninitialized',
  language: 'c'
)

subdir('src')

lib = library('clox', clox_sources)
exe = executable('clox', clox_main, link_with: lib )

subdir('tests')
