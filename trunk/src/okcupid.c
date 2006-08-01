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

#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcre.h>
#include <unistd.h>
#include <glib.h>
#define __USE_XOPEN
#define _XOPEN_SOURCE
#include <pcreposix.h>
#include "browser.h"
#include "cookies.h"
#include "common.h"
#include "blog.h"

typedef struct _okcupid_priv
{
	char tuid[32];
	const char *username;
	const char *password;
} okcupid_priv;

static bool login(blog_state *blog, bool ignorecache)
{
	char * tmpbuf;
	okcupid_priv *p = (okcupid_priv*)blog->_priv;
	CURL *c = browser_curl(blog->b);
	long retcode;
	curl_easy_setopt(c,CURLOPT_URL,"http://www.okcupid.com/login");
	char *outbuf = g_strdup_printf("username=%s&password=%s&p=%%2Fhome&submit=Login",p->username,p->password);
	curl_easy_setopt(c,CURLOPT_POSTFIELDS,outbuf);
	tmpbuf = getfile(c,CACHE_DIR DIRSEP "login", ignorecache, &retcode);
	if (strstr(tmpbuf,"Sorry, your password didn't match")!=NULL)
	{
		printf("Password failure!\n");
		return false;
	}	
	free(tmpbuf);
	return true;
}

static bool blog_init(blog_state *blog, const char *username, const char *password)
{
	blog->b = browser_init(COOKIE_FILE);
	blog->_priv = malloc(sizeof(okcupid_priv));
	okcupid_priv *p = (okcupid_priv*)blog->_priv;
	p->username = strdup(username);
	p->password = strdup(password);
	return login(blog, false);
}

static blog_entry ** get_entries(blog_state *blog, bool ignorecache)
{
	regex_t datareg; //,hrefreg,moodreg;
	regmatch_t match[4];
	int ret, index;
	blog_entry **entries = (blog_entry**)calloc(1,sizeof(blog_entry*));
	int count = 0;

	browser *b = blog->b;
	okcupid_priv *p = (okcupid_priv*)blog->_priv;
	char *tmpbuf;
	long retcode;
	int limit=0;
	CURL *c = browser_curl(b);
	while(limit<2)
	{
		c = browser_curl(b);
		curl_easy_setopt(c,CURLOPT_URL,"http://www.okcupid.com/journal");
		tmpbuf = getfile(c,CACHE_DIR DIRSEP "journal", ignorecache, &retcode);
		if (retcode == 200)
			break;
		ignorecache = true;	
		if (!login(blog, ignorecache))
			exit(EXIT_FAILURE);
		limit++;
	}
	if (limit == 2)
	{
		printf("Much failing\n");
		exit(EXIT_FAILURE);
	}	
	
	if (regcomp(&datareg, "<div class=\"journal_head\"><span class=\"journal_title\">(.+?)\n</span>.*?<script language=\"javascript\">ezdate\\((\\d+), \\d+\\);</script></div>[ \n\t]*<div class=\"journal_content\">[\n \t]*(.*?)[\n \t]*</div>", REG_DOTALL|REG_NEWLINE) != 0) {
		printf("Error while compiling pattern\n");
		exit(EXIT_FAILURE);
	}
	index = 0;
	ret = regexec(&datareg, tmpbuf, 4, match, 0);
	if (ret!=0)
		printf("No matches!\n");
	while (ret==0)
	{
		time_t indate;
		struct tm *conv_date;
		int i;
		tmpbuf[index+match[1].rm_eo] = '\0';
		tmpbuf[index+match[2].rm_eo] = '\0';
		tmpbuf[index+match[3].rm_eo] = '\0';
		entries = (blog_entry**)realloc(entries,(count+2)*sizeof(blog_entry*));
		entries[count] = (blog_entry*)malloc(sizeof(blog_entry));
		entries[count]->title = strdup(&tmpbuf[index+match[1].rm_so]);
		for (i=0;i<strlen(entries[count]->title);i++)
			if (entries[count]->title[i] == '\n')
				entries[count]->title[i] = ' ';
		entries[count]->content = strdup(&tmpbuf[index+match[3].rm_so]);

		indate = atol(&tmpbuf[index+match[2].rm_so]);
		printf("indate =%lu (from %s)\n",indate,&tmpbuf[index+match[2].rm_so]);
		conv_date = gmtime(&indate);
		memcpy(&(entries[count]->date),conv_date,sizeof(struct tm));
		
		count++;
		index += match[0].rm_eo+1;
		ret = regexec(&datareg, &tmpbuf[index], 4, match, 0);
	}
	entries[count] = NULL;
	regfree(&datareg);
	if (regcomp(&datareg, "journal\\?tuid=(\\d+)",0) != 0) {
		printf("Error while compiling pattern\n");
		exit(EXIT_FAILURE);
	}
	ret = regexec(&datareg, tmpbuf, 2, match, 0);
	regfree(&datareg);
	if (ret!=0)
	{
		printf("no tuid!\n");
		exit(EXIT_FAILURE);
	}
	char *tuid = p->tuid;
	tmpbuf[match[1].rm_eo] = '\0';
	strcpy(tuid,&tmpbuf[match[1].rm_so]);
	printf("tuid = '%s'\n",tuid);
	free(tmpbuf);

	return entries;
}

static bool blog_post(blog_state *blog, const blog_entry* entry)
{
	char *outbuf = NULL;
	long retcode;
	CURL *c = browser_curl(blog->b);
	char *title = url_format(c,entry->title);
	char *content = url_format(c,entry->content);
	outbuf = g_strdup_printf("tuid=%s&title=%s&content=%s&commentsecurity=0&commentapproval=0&postsecurity=0&formatting=0&add=Publish&trackback_username=&trackback_boardid=&trackback_url=&trackback_title=&trackback_uservar=&trackback_hideresponseline=",((okcupid_priv*)blog->_priv)->tuid,title,content);
	printf("outbuf = '%s'\n",outbuf);
	curl_easy_setopt(c,CURLOPT_URL,"http://www.okcupid.com/journal");
	curl_easy_setopt(c,CURLOPT_REFERER,"http://www.okcupid.com/jpost");
	curl_easy_setopt(c,CURLOPT_POSTFIELDS,outbuf);
	getfile(c,CACHE_DIR DIRSEP "journal", true, &retcode);
	if (entry->date.tm_year!=0)
	{
		browser *b = blog->b;
		CURL *c = browser_curl(b);
		char *tmpbuf;
		regex_t datareg;
		curl_easy_setopt(c,CURLOPT_URL,"http://www.okcupid.com/journal");
		tmpbuf = getfile(c,CACHE_DIR DIRSEP "journal", true, &retcode);
		//curl_easy_cleanup(c);
		if (regcomp(&datareg, "\\/jpost\\?edit=(\\d+)", 0) != 0) {
			printf("Error while compiling pattern\n");
			exit(EXIT_FAILURE);
		}
		regmatch_t match[2];
		int ret = regexec(&datareg, tmpbuf, 2, match, 0);
		regfree(&datareg);
		if (ret!=0)
		{
			printf("no puid!\n");
			exit(EXIT_FAILURE);
		}
		char uid[200];
		tmpbuf[match[1].rm_eo] = '\0';
		strcpy(uid,&tmpbuf[match[1].rm_so]);
		printf("uid = '%s'\n",uid);
		free(tmpbuf);
		int gmt=mktime((struct tm*)&(entry->date));
		printf("gmt=%d\n",gmt);
		free(outbuf);
		outbuf = g_strdup_printf("tuid=%s&update=1&postid=%s&postgmt=%d&title=%s&content=%s&commentsecurity=0&commentapproval=0&postsecurity=0&formatting=0&month=%d&date=%d&year=%d&hour=%d&minute=%d&ampm=%s&add=Publish",((okcupid_priv*)blog->_priv)->tuid,uid,gmt,title,content,entry->date.tm_mon,entry->date.tm_mday,entry->date.tm_year+1900,entry->date.tm_hour%12,entry->date.tm_min,entry->date.tm_hour>=12?"pm":"am");
		printf("outbuf = '%s'\n",outbuf);
		c = browser_curl(blog->b);
		curl_easy_setopt(c,CURLOPT_URL,"http://www.okcupid.com/journal");
		curl_easy_setopt(c,CURLOPT_POSTFIELDS,outbuf);
		getfile(c,NULL, false, &retcode);
	}
		
	free(outbuf);
	free(title);
	free(content);
	return true;
}

static void cleanup(blog_state *blog)
{
	browser_free(blog->b);
}

const blog_system okcupid_blog_system = {blog_init,get_entries,blog_post,cleanup};

