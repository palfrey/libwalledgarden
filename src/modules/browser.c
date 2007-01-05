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

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <curl/curl.h>
#include <assert.h>
#include <glib.h>
#include <pthread.h>
#include <stdarg.h>
#include <ltdl.h>

#include "walledgarden.h"
#include "browser.h"


struct _Request
{
	char *postfields;
	CURL *curl;
	char *url;
};


/* safe_realloc is necessary for windows
 * Most Unixes do realloc() fine with a NULL pointer, but windows segfaults on one.
 * Therefore I wrote a checker function */
void * safe_realloc(void *ptr,int size)
{
	void *ret;
	if (ptr==NULL)
		ret = calloc(1,size);
	else
		ret = realloc(ptr,size);
  	if (ret==NULL) {
		printf("Failed to allocate %d bytes\n",size);
		raise(SIGSEGV);
  	}
  	return ret;
}

bool exists(const char *filename)
{
	struct stat junk;
	return stat(filename,&junk)==0;
}

static char * readfile(const char *filename)
{
	int size = 0;
	char *ib =NULL;
	FILE *myfile = fopen(filename,"r");

	/* if something went wrong, print the error then quit */
	if (myfile == NULL) {
		perror(filename);
		exit(1);
	}

	/* Grabs 512-byte chunks of the file at a time and drops them onto our output buffer.
	 * Arbitrarily chosen chunk size. */
	while (feof(myfile)==0)
	{
		ib = (char*)safe_realloc(ib,(size+512)*sizeof(char));
		if (ib == NULL) {
			printf("Can't allocate memory chunk %d\n",size+512);
			exit(1);
		}
		//printf("Allocated memory chunk %d\n",size+512);
		memset(&ib[size],0,512);
		size+=fread(&ib[size],1,512,myfile);
	}
	fclose(myfile);
	return ib;
}

size_t nullstream( void *ptr, size_t size, size_t nmemb, void *stream)
{
	return size*nmemb;
}

static int grabs = 0;
#define MAX_GRABS 10

void _getfile(Request *req, const char *filename, bool ignorefile, long *retcode, void (*callback)(char *, void*), void *data)
{
	if (filename!=NULL)
		printf("filename %s\n",filename);	
	if (req->url!=NULL)
		printf("url %s\n",req->url);
	if (req->postfields!=NULL)
	{
		printf("getfile with postfields = %s\n",req->postfields);
		curl_easy_setopt(req->curl,CURLOPT_POSTFIELDS,req->postfields);
	}
	if (filename==NULL || !exists(filename) ||ignorefile)
	{
		FILE *curr = NULL;
		if (filename!=NULL)
		{
			curr = fopen(filename,"w");
			assert(curr!=NULL);
			curl_easy_setopt(req->curl,CURLOPT_WRITEDATA,curr);
		}	
		else
			curl_easy_setopt(req->curl,CURLOPT_WRITEFUNCTION,nullstream);
		if (grabs>MAX_GRABS)
		{
			printf("max_grabs (%d) exceeded\n",MAX_GRABS);
			exit(1);
		}
		curl_easy_perform(req->curl);
		grabs++;
		if (retcode!=NULL)
			curl_easy_getinfo(req->curl,CURLINFO_RESPONSE_CODE, retcode);
		curl_easy_cleanup(req->curl);
		if (curr!=NULL)
			fclose(curr);
	}
	printf("\n");
	if (filename==NULL)
		callback(NULL,data);
	else	
		callback(readfile(filename),data);
}


char *findandreplace(const char *inp, const replace *items, int count)
{
	int i;
	gchar ** sp = NULL, *out = NULL;
	for (i=0;i<count;i++)
	{
		sp = g_strsplit(out==NULL?inp:out,items[i].from, -1);
		out = g_strjoinv(items[i].to, sp);
		free(sp);
	}
	return out;
}

void ready_init(ThreadReady *th) {
	th->ready = false;
	pthread_cond_init(&th->cond, NULL);
	pthread_mutex_init(&th->mutex, NULL);
}

void ready_wait(ThreadReady *th) {
	pthread_mutex_lock(&th->mutex);
	while (!th->ready) {
		pthread_cond_wait(&th->cond, &th->mutex);
	}
	pthread_mutex_unlock(&th->mutex);
}

void ready_wait_time(ThreadReady *th, uint16_t seconds) {
	struct timespec timeout;
	struct timeval now;
	int ret=0;
	gettimeofday(&now,NULL);
	timeout.tv_sec = now.tv_sec + seconds;
	timeout.tv_nsec = now.tv_usec*1000;
	pthread_mutex_lock(&th->mutex);
	while (!th->ready && ret!=ETIMEDOUT) {
		ret = pthread_cond_timedwait(&th->cond, &th->mutex, &timeout);
	}
	pthread_mutex_unlock(&th->mutex);
}

void ready_done(ThreadReady *th) {
	pthread_mutex_lock(&th->mutex);
	th->ready = true;
	pthread_cond_broadcast(&th->cond);
	pthread_mutex_unlock(&th->mutex);
}

bool ready_test(ThreadReady *th) {
	bool ret;
	pthread_mutex_lock(&th->mutex);
	ret = th->ready;
	pthread_mutex_unlock(&th->mutex);
	return ret;
}

char *url_format(const char *input)
{
	return curl_escape(input,strlen(input));
}

__BEGIN_DECLS
browser * _browser_init(const char *jar)
{
	browser *ret = (browser*)malloc(sizeof(browser));
	ret->jar = (char*)malloc(strlen(jar)+1);
	strcpy(ret->jar,jar);
	return ret;
}

void _browser_free(browser* b)
{
	free(b->jar);
	free(b);
}

void blog_state_init(blog_state *blog)
{
	blog->browser_init = _browser_init;
	blog->browser_free = _browser_free;
	blog->browser_curl = _browser_curl;
	blog->browser_append_post = _browser_append_post;
	blog->browser_append_post_int = _browser_append_post_int;
	blog->browser_set_referer = _browser_set_referer;
	blog->getfile = _getfile;
	blog->b = blog->browser_init(COOKIE_FILE);
}
__END_DECLS

Request * _browser_curl(browser *b, const char *url)
{
	Request *req = (Request*)calloc(1,sizeof(Request));
	struct curl_slist *hdrs = NULL;

	req->curl = curl_easy_init();

	if (!req->curl)
		return NULL;
	curl_easy_setopt(req->curl,CURLOPT_COOKIEFILE,b->jar);
	curl_easy_setopt(req->curl,CURLOPT_COOKIEJAR,b->jar);
	//curl_easy_setopt(req->curl,CURLOPT_VERBOSE,1);
	hdrs = curl_slist_append(hdrs,"User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.0; en-US; rv:1.8.0.3) Gecko/20060426 Firefox/1.5.0.3");
	hdrs = curl_slist_append(hdrs,"Accept: text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5");
	hdrs = curl_slist_append(hdrs,"Accept-Language: en-gb,en-us;q=0.7,en;q=0.3");
	hdrs = curl_slist_append(hdrs,"Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7");
	curl_easy_setopt(req->curl,CURLOPT_HTTPHEADER,hdrs);
	curl_easy_setopt(req->curl,CURLOPT_URL,url);
	req->url = g_strdup(url);
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


void _browser_append_post(Request *req, ...)
{
	va_list ap;
	va_start (ap, req);
	int_browser_append_post(req,false,ap);
}

void _browser_append_post_int(Request *req, ...)
{
	va_list ap;
	va_start (ap, req);
	int_browser_append_post(req,true,ap);
}

void _browser_set_referer(Request *req, const char* referer)
{
	curl_easy_setopt(req->curl,CURLOPT_REFERER,referer);
}


