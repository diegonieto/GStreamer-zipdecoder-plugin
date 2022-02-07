# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstgzdec.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstgzdec.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in
libgstgzdec_la_SOURCES = gstgzdec.c gstgzdec.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstgzdec_la_CFLAGS = $(GST_CFLAGS) $(Z_CFLAGS)
libgstgzdec_la_LIBADD = $(GST_LIBS) $(Z_LIBS)
libgstgzdec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstgzdec_la_LIBTOOLFLAGS = --tag=disable-static
