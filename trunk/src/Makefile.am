AUTOMAKE_OPTIONS = foreign 1.6
SUBDIRS = modules
noinst_PROGRAMS = lj
include_HEADERS = walledgarden.h

lj_SOURCES = lj.c
lj_LDFLAGS = -lwalledgarden
modulesdir = $(libdir)/walledgarden
lib_LTLIBRARIES = libwalledgarden.la
libwalledgarden_la_SOURCES = walledgarden.c modules/browser.c
libwalledgarden_la_CFLAGS = -Imodules/ $(GLIB_CFLAGS) -Wall -Werror -O0 -DMODULESDIR=\"$(modulesdir)\"
libwalledgarden_la_LDFLAGS = -lltdl -lpcreposix $(PCRE_LIBS) 
AM_CFLAGS =$(GLIB_CFLAGS) $(RAPTOR_CFLAGS) -Wall -Werror -O0 -DMODULESDIR=\"$(modulesdir)\"
AM_LDFLAGS = $(GLIB_LIBS) $(CURL_LIBS) $(RAPTOR_LIBS) -Wl,--as-needed


