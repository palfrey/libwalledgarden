AUTOMAKE_OPTIONS = foreign 1.6
SUBDIRS = modules
bin_PROGRAMS = lj
lj_SOURCES = lj.c browser.c
CFLAGS +=$(GLIB_CFLAGS) $(RAPTOR_CFLAGS) -Wall -Werror -O0
LDFLAGS += $(GLIB_LIBS) $(PCRE_LIBS) $(CURL_LIBS) $(RAPTOR_LIBS) -lpcreposix
