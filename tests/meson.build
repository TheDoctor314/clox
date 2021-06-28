test_sources = files([
  'main.c',
  'scanner_tests.c',
  'table_tests.c',
])

e = executable('unit_tests', test_sources,link_with: lib, include_directories: incdir)

test('Unit test', e)
