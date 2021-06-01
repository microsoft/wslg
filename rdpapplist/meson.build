project('rdpapplist',
    'c',
    version : '1.0.0',
)

name_rdpapplist = meson.project_name()
version_rdpapplist = meson.project_version()

config_h = configuration_data()

dir_prefix = get_option('prefix')

dir_lib = join_paths(dir_prefix, get_option('libdir'))
install_lib_dir_rdpapplist = join_paths(dir_lib, name_rdpapplist)
config_h.set_quoted('RDPAPPLIST_MODULEDIR',
     install_lib_dir_rdpapplist)

dir_inc = join_paths(dir_prefix, get_option('includedir'))
install_inc_dir_rdpapplist = join_paths(dir_inc, name_rdpapplist)
config_h.set_quoted('RDPAPPLIST_INCLUDEDIR',
     install_inc_dir_rdpapplist)

dep_frdp = dependency('freerdp2', version: '>= 2.0.0', required: false)
if not dep_frdp.found()
    error('rdpapplist requires freerdp2 which was not found.')
endif

dep_winpr = dependency('winpr2', version: '>= 2.0.0', required: false)
if not dep_winpr.found()
    error('rdpapplist requires winpr2 which was not found.')
endif

configure_file(output: 'rdpapplist_config.h',
     configuration: config_h,
     install_dir: install_inc_dir_rdpapplist)

subdir('server')

pkgconfig = import('pkgconfig')
pkgconfig.generate(
    filebase: name_rdpapplist,
    version: version_rdpapplist,
    name: 'Application list Plugin for FreeRDP',
    description: 'Header files for Application list Plugin for FreeRDP',
)

install_headers(
    'rdpapplist_protocol.h',
    'rdpapplist_server.h',
    subdir: name_rdpapplist,
)
