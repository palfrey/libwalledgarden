#ifndef COMMON_H
#define COMMON_H 1

#define CACHE_DIR "cache"
#define COOKIE_FILE CACHE_DIR DIRSEP "cookies"
#if defined(_WIN32) && !defined(__CYGWIN__)
char * strptime (const char *buf, const char *format, struct tm *timeptr);
#else
#ifndef __USE_XOPEN
#define __USE_XOPEN
#define _XOPEN_SOURCE /* glibc2 needs this */
#endif
#include <time.h>
#endif

#include <stdbool.h>
#include <curl/curl.h>
bool exists(const char *filename);
char *getfile(CURL *c, const char *filename, bool ignorefile, long *retcode);

char *url_format(CURL *c, const char *input);

#endif
