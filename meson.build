project('morsesrc', 'c',
  default_options: ['warning_level=3', 'optimization=0', 'debug=false'])
  
  # Add configuration data
conf_data = configuration_data()
conf_data.set_quoted('BUILD_DATE', run_command('date', '+%Y-%m-%d', check: true).stdout().strip())

# Configure the build header
configure_file(
  output: 'config.h',
  configuration: conf_data
)

# Add builddir include path
add_project_arguments('-I' + meson.current_build_dir(), language: 'c')

gst_dep = dependency('gstreamer-1.0')
gstbase_dep = dependency('gstreamer-base-1.0')
gstaudio_dep = dependency('gstreamer-audio-1.0')
glib_dep = dependency('glib-2.0')
gobject_dep = dependency('gobject-2.0')

cc = meson.get_compiler('c')
math_lib = cc.find_library('m', required: true)

# Just use libdir directly
install_dir = get_option('libdir')

# Plugin source
plugin_src = [
  'src/gstmorsesrc.c',
]

# Build the plugin
libgstmorsesrc = shared_library('gstmorsesrc', plugin_src,
  dependencies: [gstaudio_dep, gstbase_dep, gst_dep, glib_dep, gobject_dep, math_lib],
  install: true,
  install_dir: install_dir
)
