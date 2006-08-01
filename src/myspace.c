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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcre.h>
#include <unistd.h>
#define __USE_XOPEN
#define _XOPEN_SOURCE
#include <time.h>
#include <pcreposix.h>
#include "browser.h"
#include "cookies.h"
#include "common.h"
#include "blog.h"
#include <glib.h>

typedef struct _myspace_priv
{
	const char *username;
	const char *password;
	const char *token;
} myspace_priv;

static bool login(blog_state *blog, bool ignorecache)
{
	myspace_priv *p = (myspace_priv*)blog->_priv;
	char * tmpbuf, *url;
	regex_t datareg;
	int ret;
	regmatch_t match[3];
	char errbuf[255];

	if (regcomp(&datareg, "<form action=\"(http://login.myspace.com/index.cfm\\?fuseaction=login.process&MyToken=([^\\\"]*))\" method=\"post\" name=\"theForm\" id=\"theForm\">", 0) != 0) {
		printf("Error while compiling pattern\n");
		exit(EXIT_FAILURE);
	}
	while(1)
	{
		CURL *c = browser_curl(blog->b);
		curl_easy_setopt(c,CURLOPT_URL,"http://www.myspace.com");
		tmpbuf = getfile(c,CACHE_DIR DIRSEP "login", ignorecache, NULL);

		ret = regexec(&datareg, tmpbuf, 3, match, 0);
		if (ret != 0) {
			regerror(ret, &datareg, errbuf, 255);
			printf("Error while matching pattern (login): %s\n", errbuf);
			if (ignorecache)
				return false;
			ignorecache = true;	
			free(tmpbuf);
		}
		else
			break;
	}		
	tmpbuf[match[1].rm_eo] = '\0';
	p->token = strdup(&tmpbuf[match[2].rm_so]);
	url = strdup(&tmpbuf[match[1].rm_so]);
	printf("token is %s\n",p->token);
	free(tmpbuf);

	CURL *c = browser_curl(blog->b);
	curl_easy_setopt(c,CURLOPT_URL,url);
	char *outbuf = g_strdup_printf("email=%s&password=%s&Remember=Remember&ctl00%%24Main%%24SplashDisplay%%24login%%24loginbutton.x=19&ctl00%%24Main%%24SplashDisplay%%24login%%24loginbutton.y=11",url_format(c,p->username),url_format(c,p->password));
	curl_easy_setopt(c,CURLOPT_POSTFIELDS,outbuf);
	long retcode=0;
	tmpbuf = getfile(c,CACHE_DIR DIRSEP "logged",ignorecache,&retcode);
	free(tmpbuf);
	if (retcode!=0 && retcode!=302)
		return false;
	else	
		return true;
}

static bool blog_init(blog_state *blog, const char *username, const char *password)
{
	blog->b = browser_init(COOKIE_FILE);
	blog->_priv = malloc(sizeof(myspace_priv));
	myspace_priv *p = (myspace_priv*)blog->_priv;

	p->username = strdup(username);
	p->password = strdup(password);
	return login(blog, false);
}

static blog_entry ** get_entries(blog_state *blog, bool ignorecache)
{
	browser *b = blog->b;
	char * tmpbuf;
	char *url;
	myspace_priv *p = (myspace_priv*)blog->_priv;
	blog_entry **entries = (blog_entry**)calloc(1,sizeof(blog_entry*));
	int count = 0;

	regex_t datareg,hrefreg,moodreg;
	regmatch_t match[3];
	int ret;
	char errbuf[255];
	if (regcomp(&datareg, "(http://blog.myspace.com/index.cfm\\?fuseaction=blog.ListAll&amp;friendID=\\d+)", 0) != 0) {
		printf("Error while compiling pattern\n");
		exit(EXIT_FAILURE);
	}
	url = g_strdup_printf("http://home.myspace.com/index.cfm?fuseaction=user&MyToken=%s",p->token);
	while(1)
	{
		CURL *c = browser_curl(blog->b);
		curl_easy_setopt(c,CURLOPT_URL,url);
		tmpbuf = getfile(c,CACHE_DIR DIRSEP "home",ignorecache,NULL);
		ret = regexec(&datareg, tmpbuf, 2, match, 0);
		if (ret != 0) {
			regerror(ret, &datareg, errbuf, 255);
			printf("Error while matching pattern (blog): %s\n", errbuf);
			if (ignorecache)
				exit(EXIT_FAILURE);
			ignorecache = true;
			free(tmpbuf);
		}
		else
			break;
	}
	tmpbuf[match[1].rm_eo] = '\0';
	free(url);
	url = strdup(&tmpbuf[match[1].rm_so]);
	free(tmpbuf);
	
	CURL *c = browser_curl(b);
	curl_easy_setopt(c,CURLOPT_URL,url);
	tmpbuf = getfile(c,CACHE_DIR DIRSEP "blog", ignorecache, NULL);
	free(url);
	if (regcomp(&datareg, "<p class=\"blog([^\\\"]*)\">(.*?)</p>", REG_DOTALL|REG_NEWLINE) != 0) {
		printf("Error while compiling pattern\n");
		exit(EXIT_FAILURE);
	}
	if (regcomp(&hrefreg, "<a href=\"([^\\\"]*)\">(.*?)</a>", REG_DOTALL|REG_NEWLINE) != 0) {
		printf("Error while compiling pattern\n");
		exit(EXIT_FAILURE);
	}
	if (regcomp(&moodreg, "(.*?)[\r\n \t]*<br />Current mood: <img src=\"([^\"]+)\" align=\"absmiddle\"> ([^\r\n]+)", REG_DOTALL|REG_NEWLINE) != 0) {
		printf("Error while compiling pattern\n");
		exit(EXIT_FAILURE);
	}
	int index = 0;
	//printf("dist = %d (%9s)\n",match[0].rm_eo,&tmpbuf[index]);
	ret = regexec(&datareg, &tmpbuf[index], 3, match, 0);
	if (ret !=0)
		printf("No entries!\n");
	entries = (blog_entry**)realloc(entries,(count+2)*sizeof(blog_entry*));
	entries[count] = (blog_entry*)malloc(sizeof(blog_entry));
	while (ret==0)
	{
		struct tm out;
		const char * cat = &tmpbuf[index+match[1].rm_so];
		char * content = &tmpbuf[index+match[2].rm_so];
		tmpbuf[index+match[1].rm_eo] = '\0';
		tmpbuf[index+match[2].rm_eo] = '\0';
		if (strcmp(cat,"TimeStamp")==0)
		{
			char *ret = strptime(content,"%A, %b %d, %Y",&out);
			char outstr[200];
			if (ret == NULL || ret[0]!='\0')
			{
				printf("Ret = %s\n",ret);
				exit(1);
			}
			strftime(outstr,sizeof(outstr),"%Y-%m-%d",&out);
			//printf("Time = %s (from %s)\n\n",outstr,content);
		}
		else if (strcmp(cat,"Subject")==0)
		{
			int end;
			regmatch_t mood[4];
			ret = regexec(&moodreg, content, 4, mood, 0);
			entries = (blog_entry**)realloc(entries,(count+2)*sizeof(blog_entry*));
			entries[count] = (blog_entry*)malloc(sizeof(blog_entry));
			if (ret==0)
			{
				const char *url = &content[mood[2].rm_so];
				const char *name = &content[mood[3].rm_so];
				content[mood[1].rm_eo] = '\0';
				content[mood[2].rm_eo] = '\0';
				content[mood[3].rm_eo] = '\0';
				content = &content[mood[1].rm_so];
				printf("mood = %s, image=%s\n",name,url);
			}
			while (content[0]==' ' || content[0]=='\r' || content[0]=='\t'|| content[0]=='\n')
				content++;
			end = strlen(content)-1;
			//printf("c[e] = %d end=%d\n",content[end],end);	
			while (content[end]==' ' || content[end]=='\r' || content[end]=='\t'|| content[end]=='\n')
				end--;
			//printf("c[e] = %d end=%d\n",content[end],end);	
			content[end+1]='\0';
			entries[count]->title = strdup(content);
			printf("Subject(%d): %s\n",count,content);
		}
		else if (strcmp(cat,"ContentInfo")==0)
		{
			regmatch_t link[3];
			int c_index = 0;
			ret = regexec(&hrefreg, &content[c_index], 3, link, 0);
			while (ret==0)
			{
				//const char * url = &content[c_index+link[1].rm_so];
				const char * name = &content[c_index+link[2].rm_so];
				content[c_index+link[1].rm_eo] = '\0';
				content[c_index+link[2].rm_eo] = '\0';
				char *st = strptime(name,"<b>%I:%M %p</b>",&out);
				if (st!=NULL)
				{
					char outstr[200];
					out.tm_sec = 0;
					strftime(outstr,sizeof(outstr),"%Y-%m-%d %I:%M:%S %p",&out);
					printf("Time(%d) = %s (from %s)\n",count,outstr,name);
					memcpy(&(entries[count]->date),&out,sizeof(struct tm));
					count++;
				}
				/*else
					printf("url=%s, name=%s\n",url,name);*/
					
				c_index += link[0].rm_eo+1;
				ret = regexec(&hrefreg, &content[c_index], 3, link, 0);
			}
			printf("\n");
		}
		else if (strcmp(cat,"Content")==0)
			entries[count]->content = strdup(content);
		else
			printf("cat=%s, content = %s\n",cat, content);

		index += match[0].rm_eo+1;
		//printf("dist = %d (%9s)\n",match[0].rm_eo,&tmpbuf[index]);
		ret = regexec(&datareg, &tmpbuf[index], 3, match, 0);
	}
	return entries;
}

static void cleanup(blog_state *blog)
{
	browser_free(blog->b);
}

static bool blog_post(blog_state *blog, const blog_entry* entry)
{
	return true;
}

const blog_system myspace_blog_system = {blog_init,get_entries,blog_post, cleanup};

