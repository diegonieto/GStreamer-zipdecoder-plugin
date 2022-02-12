plugin_c_args = ['-DHAVE_CONFIG_H']

cdata = configuration_data()
cdata.set_quoted('PACKAGE_VERSION', gst_version)
cdata.set_quoted('PACKAGE', 'gst-template-plugin')
cdata.set_quoted('GST_LICENSE', 'LGPL')
cdata.set_quoted('GST_API_VERSION', api_version)
cdata.set_quoted('GST_PACKAGE_NAME', 'GStreamer template Plug-ins')
cdata.set_quoted('GST_PACKAGE_ORIGIN', 'https://gstreamer.freedesktop.org')
configure_file(output : 'config.h', configuration : cdata)

zdep = dependency('zlib', version : '>=1.2.8')

# The gzdec Plugin
 gstgzdec_sources = [
  'src/gstgzdec.c',
  ]

gstgzdec = library('gstgzdec',
  gstgzdec_sources,
  c_args: plugin_c_args,
  dependencies : [gst_dep, gstbase_dep, zdep],
  install : true,
  install_dir : plugins_install_dir,
)