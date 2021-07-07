project('nuklear', 'c', version : '4.07.0')

inc = include_directories('include')

nuklear_dep = declare_dependency(
    include_directories : inc
)

meson.override_dependency('nuklear', nuklear_dep)