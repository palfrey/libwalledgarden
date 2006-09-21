/*
 * Copyright (C) Tom Parker
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
#include "okcupid.h"
#include "myspace.h"
#include "blog.h"
#include "common.h"
#include <glib.h>
#include <pthread.h>

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

void getfile(Request *req, const char *filename, bool ignorefile, long *retcode, void (*callback)(char *, void*), void *data)
{
	if (req->postfields!=NULL)
	{
		printf("getfile with postfields = %s\n",req->postfields);
		curl_easy_setopt(req->curl,CURLOPT_POSTFIELDS,req->postfields);
	}
	if (filename!=NULL)
		printf("filename %s\n",filename);	
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
		curl_easy_perform(req->curl);
		if (retcode!=NULL)
			curl_easy_getinfo(req->curl,CURLINFO_RESPONSE_CODE, retcode);
		curl_easy_cleanup(req->curl);
		if (curr!=NULL)
			fclose(curr);
	}
	if (filename==NULL)
		callback(NULL,data);
	else	
		callback(readfile(filename),data);
}

typedef struct _char_map {
	const char in;
	const char *out;
} char_map;

const blog_system* blog_choose(const char*name)
{
	if (strcmp(name,"okcupid")==0)
		return &okcupid_blog_system;
	else if (strcmp(name,"myspace")==0)
		return &myspace_blog_system;
	else
		return NULL;
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
