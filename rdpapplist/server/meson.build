dep_frdp_server = dependency('freerdp-server3', version: '>= 3.0.0', required: false)
if not dep_frdp_server.found()
    dep_frdp_server = dependency('freerdp-server2', version: '>= 2.0.0', required: false)
    if not dep_frdp_server.found()
        error('librapapplist-server requires freerdp-server2 or 3 which was not found.')
    endif
endif

deps_librdpapplist_server = [
    dep_frdp,
    dep_frdp_server,
    dep_winpr,
]

srcs_librdpapplist_server = [
    '../rdpapplist_common.c',
    'rdpapplist_main.c',
]

incs_common_server = [
    '../',
]

plugin_rdpapplist_server = shared_library(
    'librdpapplist-server',
    srcs_librdpapplist_server,
    include_directories: incs_common_server,
    dependencies: deps_librdpapplist_server,
    name_prefix: '',
    install: true,
    install_dir: install_lib_dir_rdpapplist,
)
