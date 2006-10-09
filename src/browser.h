/*
 * Copyright (C) Tom Parker
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.

 * Authors:
 *      Tom Parker <palfrey@tevp.net>
 */

#ifndef BROWSER_H_
#define BROWSER_H_

#include <curl/curl.h>
#include <stdbool.h>
#include <stdint.h>
#include <curl/curl.h>

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

typedef struct _browser
{
	char *jar;
} browser;

typedef struct _Request Request;

browser * browser_init(const char *jar);
void browser_free(browser* b);
Request * browser_curl(browser *b, const char *url);
void browser_append_post(Request *req, ...) __attribute__((__sentinel__(0)));
void browser_append_post_int(Request *req, ...) __attribute__((__sentinel__(0)));
void browser_set_referer(Request *req, const char* referer);

#ifdef _WIN32
#define DIRSEP "\\"
#else
#define DIRSEP "/"
#endif

bool exists(const char *filename);
void getfile(Request *req, const char *filename, bool ignorefile, long *retcode, void (*callback)(char *, void *data), void *data);

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
