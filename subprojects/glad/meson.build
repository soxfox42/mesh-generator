project('glad', 'c', version : '0.1.34')

cc = meson.get_compiler('c')
ld_dep = cc.find_library('dl', required : false)

inc = include_directories('include')
glad = static_library('glad', 'src/glad.c', include_directories : inc, dependencies : [ld_dep])

glad_dep = declare_dependency(
    link_with : glad,
    include_directories : inc
)

meson.override_dependency('glad', glad_dep)
