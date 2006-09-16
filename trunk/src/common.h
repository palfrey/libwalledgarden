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
#include <stdint.h>
#include <curl/curl.h>

bool exists(const char *filename);
void getfile(CURL *c, const char *filename, bool ignorefile, long *retcode, void (*callback)(char *, void *data), void *data);

typedef struct _replace
{
	const char *from;
	const char *to;
} replace;

char *findandreplace(const char *inp, const replace *using, const int count);

typedef struct {
	bool ready;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
} ThreadReady;

void ready_init(ThreadReady *th);
void ready_wait(ThreadReady *th);
void ready_wait_time(ThreadReady *th, uint16_t seconds);
void ready_done(ThreadReady *th);
bool ready_test(ThreadReady *th);
#endif
