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
#include "okcupid.h"
#include "blog.h"
#include "common.h"
#include <glib.h>

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

char *getfile(CURL *c, const char *filename, bool ignorefile, long *retcode)
{
	if (filename==NULL || !exists(filename) ||ignorefile)
	{
		FILE *curr = NULL;
		if (filename!=NULL)
		{
			curr = fopen(filename,"w");
			printf("filename %s\n",filename);
			assert(curr!=NULL);
			curl_easy_setopt(c,CURLOPT_WRITEDATA,curr);
		}	
		else
			curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,nullstream);
		curl_easy_perform(c);
		if (retcode!=NULL)
			curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE, retcode);
		curl_easy_cleanup(c);
		if (curr!=NULL)
			fclose(curr);
	}
	if (filename==NULL)
		return NULL;
	else	
		return readfile(filename);
}

typedef struct _char_map {
	const char in;
	const char *out;
} char_map;

char *url_format(CURL *c,const char *input)
{
	/*static const char_map mapping[] = {
								{' ', "+" },
								{'\n',"%0A"},
								{'\r',"%0D"},
								{',',"%2C"},
								{'\'',"%27"},
								{0}
	};

	char *temp = calloc((strlen(input)*3)+1,sizeof(char));
	const char_map* curr = &(mapping[0]);
	printf("reformatting %s\n",input);
	strcpy(temp,input);
	while (curr->out!=NULL)
	{
		char *ind = strchr(temp,curr->in);
		while (ind!=NULL)
		{
			int idx = ind-temp;
			memmove(&(temp[idx])+strlen(curr->out),&temp[idx]+1,strlen(&(temp[idx])+1));
			memcpy(&temp[idx],curr->out,strlen(curr->out));
			ind = strchr(temp,curr->in);
			//printf("reformatting %s\n",temp);
		}	
		curr++;
	}
	return temp;*/
	return curl_escape(input,strlen(input));
}

const blog_system* blog_choose(const char*name)
{
	return &okcupid_blog_system;
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


