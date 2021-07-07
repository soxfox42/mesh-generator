project('itp_custom', 'c', default_options : 'default_library=static')

gl_dep = dependency('gl')
glfw_dep = dependency('glfw3', required : false)
if not glfw_dep.found()
    cmake = import('cmake')
    glfw_subproject = cmake.subproject('glfw')
    glfw_dep = glfw_subproject.dependency('glfw')
endif
glad_dep = dependency('glad')
cglm_dep = dependency('cglm')
nuklear_dep = dependency('nuklear')
noise_dep = dependency('noise')

inc = include_directories('include')

subdir('src')

executable(
    'itp_custom',
    sources,
    dependencies : [
        gl_dep,
        glfw_dep,
        glad_dep,
        cglm_dep,
        nuklear_dep,
        noise_dep
    ],
    include_directories : inc
)
