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

#include <curl/curl.h>
#include <string.h>
#include "browser.h"
#include <stdlib.h>
#include <stdarg.h>
#include <glib.h>

char *url_format(const char *input)
{
	return curl_escape(input,strlen(input));
}

browser * browser_init(const char *jar)
{
	browser *ret = (browser*)malloc(sizeof(browser));
	ret->jar = (char*)malloc(strlen(jar)+1);
	strcpy(ret->jar,jar);
	return ret;
}

void browser_free(browser* b)
{
	free(b->jar);
	free(b);
}

Request * browser_curl(browser *b, const char *url)
{
	Request *req = (Request*)calloc(1,sizeof(Request));
	struct curl_slist *hdrs = NULL;

	req->curl = curl_easy_init();

	if (!req->curl)
		return NULL;
	curl_easy_setopt(req->curl,CURLOPT_COOKIEFILE,b->jar);
	curl_easy_setopt(req->curl,CURLOPT_COOKIEJAR,b->jar);
	curl_easy_setopt(req->curl,CURLOPT_VERBOSE,1);
	hdrs = curl_slist_append(hdrs,"User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.0; en-US; rv:1.8.0.3) Gecko/20060426 Firefox/1.5.0.3");
	hdrs = curl_slist_append(hdrs,"Accept: text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5");
	hdrs = curl_slist_append(hdrs,"Accept-Language: en-gb,en-us;q=0.7,en;q=0.3");
	hdrs = curl_slist_append(hdrs,"Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7");
	curl_easy_setopt(req->curl,CURLOPT_HTTPHEADER,hdrs);
	curl_easy_setopt(req->curl,CURLOPT_URL,url);
	//curl_slist_free_all(hdrs);
	return req;
}

static void int_browser_append_post(Request *req, bool ints, va_list ap)
{
	const gchar *key;
	char *buf = NULL;
	
	do
	{
		key = va_arg (ap, char*);
		if (key)
		{
			char *temp,*value;
			key = url_format(key);
			if (ints)
				temp = g_strdup_printf("%s=%d",key,va_arg (ap,int));
			else {
				value = url_format(va_arg (ap,char*));
				temp = g_strdup_printf("%s=%s",key,value);
			}
			if (buf == NULL)
				buf = temp;
			else
			{
				char *oldbuf = buf;
				buf = g_strjoin("&",oldbuf,temp, NULL);
				free(oldbuf);
				free(temp);
			}
		}
	} while (key);
	va_end (ap);
	if (req->postfields == NULL)
		req->postfields = buf;
	else
	{
		char *oldbuf = req->postfields;
		req->postfields = g_strjoin("&",oldbuf,buf,NULL);
		free(oldbuf);
		free(buf);
	}
}


void browser_append_post(CURL *c, ...)
{
	va_list ap;
	va_start (ap, c);
	int_browser_append_post(c,false,ap);
}

void browser_append_post_int(CURL *c, ...)
{
	va_list ap;
	va_start (ap, c);
	int_browser_append_post(c,true,ap);
}


