plugin_LTLIBRARIES = libgstgzdec.la

# sources used to compile this plug-in
libgstgzdec_la_SOURCES = gstgzdec.c gstgzdec.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstgzdec_la_CFLAGS = $(GST_CFLAGS)
libgstgzdec_la_LIBADD = $(GST_LIBS)
libgstgzdec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -lz
libgstgzdec_la_LIBTOOLFLAGS = --tag=disable-static
