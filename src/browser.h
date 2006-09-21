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

typedef struct _browser
{
	char *jar;
} browser;

typedef struct _Request
{
	char *postfields;
	CURL *curl;
} Request;

browser * browser_init(const char *jar);
void browser_free(browser* b);
Request * browser_curl(browser *b, const char *url);
void browser_append_post(CURL *c, ...) __attribute__((__sentinel__(0)));
void browser_append_post_int(CURL *c, ...) __attribute__((__sentinel__(0)));
char *url_format(const char *input);

#ifdef _WIN32
#define DIRSEP "\\"
#else
#define DIRSEP "/"
#endif

#endif
