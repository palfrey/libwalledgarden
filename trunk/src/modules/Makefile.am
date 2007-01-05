AM_CFLAGS =$(GLIB_CFLAGS) -Wall -Werror -O0 -I../
AM_LDFLAGS = $(GLIB_LIBS) $(PCRE_LIBS) -lpcreposix

modulesdir = $(libdir)/walledgarden
modules_LTLIBRARIES = libmyspace.la libokcupid.la libblogcore.la

libmyspace_la_SOURCES = myspace.c
libmyspace_la_LDFLAGS = -no-undefined -module -export-symbols-regex "libmyspace.*" -export-dynamic
libokcupid_la_SOURCES = okcupid.c
libokcupid_la_LDFLAGS = -no-undefined -module -export-symbols-regex "libokcupid.*" -export-dynamic
libblogcore_la_SOURCES = browser.c
libokcupid_la_LDFLAGS = -no-undefined -module -export-dynamic

