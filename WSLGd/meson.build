project('WSLGd', 'cpp',
  version : '0.1',
  default_options : ['cpp_std=c++17'])

config_h = configuration_data()

dep_winpr = dependency('winpr2', version: '>= 2.0.0', required: false)
if dep_winpr.found()
  config_h.set('HAVE_WINPR2', '1')
endif

configure_file(output: 'config.h', configuration: config_h)

executable('WSLGd',
           'main.cpp',
           'ProcessMonitor.cpp',
           dependencies: dep_winpr,
           link_args: '-lcap',
           install : true)
