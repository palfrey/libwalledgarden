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
#include "blog.h"

typedef struct _okcupid_priv
{
	char tuid[32];
	const char *username;
	const char *password;
} okcupid_priv;

typedef struct
{
	void(*callback)(bool, void *);
	void *user_data;
} login_struct;

static void parse_login(char *tmpbuf, void *data)
{
	login_struct *ls = data;
	//if (strstr(tmpbuf,"Sorry, your password didn't match")!=NULL)
	if (strstr(tmpbuf,"I already have an account")!=NULL)
	{
		printf("Password failure!\n");
		ls->callback(false, ls->user_data);
	}	
	free(tmpbuf);
	ls->callback(true, ls->user_data);
}

static void login(blog_state *blog, bool ignorecache, void(*callback)(bool, void *), void* data)
{
	okcupid_priv *p = (okcupid_priv*)blog->_priv;
	Request *req = browser_curl(blog->b,"http://www.okcupid.com/login");
	login_struct *ls = (login_struct*)malloc(sizeof(login_struct));
	ls->callback = callback;
	ls->user_data = data;
	browser_append_post(req,
		"username",p->username,
		"password",p->password,
		"p","/home",
		"submit","login",
		NULL);
	getfile(req,CACHE_DIR DIRSEP "login", ignorecache, NULL, parse_login, ls);
}

static void blog_init(blog_state *blog, const char *username, const char *password, void(*callback)(bool,void*))
{
	blog->b = browser_init(COOKIE_FILE);
	blog->_priv = malloc(sizeof(okcupid_priv));
	okcupid_priv *p = (okcupid_priv*)blog->_priv;
	p->username = strdup(username);
	p->password = strdup(password);
	login(blog, true, callback,NULL);
}

typedef struct
{
	int limit;
	long retcode;
	blog_state *blog;
	bool ignorecache;
	void (*callback)(blog_entry **);
	bool handle_data;
} journal_storage;

static void do_entry(char *tmpbuf, void *data);

static void get_entries(blog_state *blog, bool ignorecache, void (*callback)(blog_entry **))
{
	journal_storage *js = (journal_storage*)calloc(1,sizeof(journal_storage));
	js->blog = blog;
	js->ignorecache = ignorecache;
	js->callback = callback;
	js->handle_data = false;
	js->limit = 0;
	do_entry(NULL, js);
}

static void entry_login(bool ok, void *data)
{
	journal_storage *js = data;
	if (!ok)
	{
		printf("Much failing\n");
		exit(EXIT_FAILURE);
	}
	do_entry(NULL,js);
}

static void do_entry(char *tmpbuf, void *data)
{
	journal_storage *js = data;
	while(1)
	{
		if (js->handle_data)
		{
			if (js->retcode == 200)
				break;
			js->ignorecache = true;	
			js->handle_data = false;
			login(js->blog, js->ignorecache, entry_login, js);
			return;
		}
		if(js->limit==2)
			break;
		
		Request *req  = browser_curl(js->blog->b,"http://www.okcupid.com/journal");
		js->limit++;
		js->handle_data = true;
		getfile(req,CACHE_DIR DIRSEP "journal", js->ignorecache, &js->retcode,do_entry,js);
		return;
	}
	if (js->limit == 2 && js->retcode!=200)
	{
		printf("Much failing\n");
		exit(EXIT_FAILURE);
	}	
	
	regex_t datareg;
	okcupid_priv *p = (okcupid_priv*)js->blog->_priv;
	regmatch_t match[4];
	int ret, index;
	blog_entry **entries = (blog_entry**)calloc(1,sizeof(blog_entry*));
	int count = 0;

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

	js->callback(entries);
}

typedef struct
{
	blog_state *blog;
	const blog_entry* entry;
	void (*callback)(bool);
} post_data;

static void date_set(char *tmpbuf, void *data);

static void blog_post(blog_state *blog, const blog_entry* entry, void (*callback)(bool))
{
	//char *outbuf = NULL;
	long retcode;
	Request *req = browser_curl(blog->b,"http://www.okcupid.com/journal");
	post_data * pd = (post_data*)malloc(sizeof(post_data));
	pd->blog = blog;
	pd->entry = entry;
	pd->callback = callback;
	browser_append_post(req,
		"tuid",((okcupid_priv*)blog->_priv)->tuid,
		"title", entry->title,
		"content",entry->content,
		"commentsecurity","0",
		"commentapproval","0",
		"postsecurity","0",
		"formatting","0",
		"add","Publish",
		"trackback_username","",
		"trackback_boardid","",
		"trackback_url","",
		"trackback_title","",
		"trackback_uservar","",
		"trackback_hideresponseline","",
		NULL);
	browser_set_referer(req,"http://www.okcupid.com/jpost");
	getfile(req,CACHE_DIR DIRSEP "journal", true, &retcode, date_set, pd);
}

static void more_date_set(char *tmpbuf, void *data);
static void end_post(char *tmpbuf, void *data);

static void date_set(char *tmpbuf, void *data)
{
	post_data *pd = data;
	free(tmpbuf);

	if (pd->entry->date.tm_year!=0)
	{
		browser *b = pd->blog->b;
		Request *req = browser_curl(b,"http://www.okcupid.com/journal");
		long retcode;
		getfile(req,CACHE_DIR DIRSEP "journal", true, &retcode, more_date_set, pd);
	}
	else	
		end_post(NULL,pd);
}

static void more_date_set(char *tmpbuf, void *data)
{
	post_data *pd = data;
	regex_t datareg;
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
	long retcode;
	tmpbuf[match[1].rm_eo] = '\0';
	strcpy(uid,&tmpbuf[match[1].rm_so]);
	printf("uid = '%s'\n",uid);
	free(tmpbuf);

	int gmt=mktime((struct tm*)&(pd->entry->date));
	printf("gmt=%d\n",gmt);
	//free(outbuf);
	Request *req = browser_curl(pd->blog->b,"http://www.okcupid.com/journal");
	browser_append_post(req,
		"tuid",((okcupid_priv*)pd->blog->_priv)->tuid,
		"update","1",
		"postid",uid,
		"postgmt",g_strdup_printf("%d",gmt),
		"title", pd->entry->title,
		"content",pd->entry->content,
		"commentsecurity","0",
		"commentapproval","0",
		"postsecurity","0",
		"formatting","0",
		"add","Publish",
		"trackback_username","",
		"trackback_boardid","",
		"trackback_url","",
		"trackback_title","",
		"trackback_uservar","",
		"trackback_hideresponseline","",
		NULL);
	getfile(req,NULL, false, &retcode, end_post, pd);
}

static void end_post(char *tmpbuf, void *data)
{
	post_data *pd = data;
	free(tmpbuf);
		
	free(pd);
	pd->callback(true);
}

static void cleanup(blog_state *blog)
{
	browser_free(blog->b);
}

const blog_system okcupid_blog_system = {blog_init,get_entries,blog_post,cleanup};

