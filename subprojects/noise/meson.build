project('noise', 'c')

inc = include_directories('include')
noise = static_library('noise', 'src/noise1234.c', include_directories : inc)

noise_dep = declare_dependency(
    link_with : noise,
    include_directories : inc
)

meson.override_dependency('noise', noise_dep)
