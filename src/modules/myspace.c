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
#include "walledgarden.h"
#include <glib.h>

#define blog_name libmyspace_LTX_blog_name
#define functions libmyspace_LTX_functions

typedef struct _myspace_priv
{
	const char *username;
	const char *password;
	const char *token;
} myspace_priv;

typedef struct
{
	void(*callback)(bool, void *);
	void *user_data;
	bool ignorecache;
	blog_state *b;
	long retcode;
} login_struct;

static void check_code(char *tmpbuf, void *data)
{
	login_struct *ls = data;
	free(tmpbuf);
	//if (ls->retcode!=0 && ls->retcode!=302)
	if (strstr(tmpbuf,"Member Login")!=NULL)
	{
		printf("login failure (%ld)\n",ls->retcode);
		if (ls->callback!=NULL)
			ls->callback(false,ls->user_data);
	}
	else if (ls->callback!=NULL)
		ls->callback(true,ls->user_data);
}

static void parse_login(char *tmpbuf, void *data)
{
	char *url;
	int ret;
	regmatch_t match[3];
	char errbuf[255];
	regex_t datareg, already;

	login_struct *ls = data;
	blog_state *blog = ls->b;
	myspace_priv *p = (myspace_priv*)blog->_priv;

	if (regcomp(&already, "http://collect.myspace.com/index.cfm\\?fuseaction=signout&MyToken=([^\\\"]*)\"", 0) != 0) {
		printf("Error while compiling pattern (already)\n");
		exit(EXIT_FAILURE);
	}
	ret = regexec(&already, tmpbuf, 2, match, 0);
	if (ret == 0)
	{
		printf("already logged in\n");
		tmpbuf[match[1].rm_eo] = '\0';
		p->token = strdup(&tmpbuf[match[1].rm_so]);
		printf("token is %s\n",p->token);
		ls->retcode = 302;
		check_code(NULL,data);
		return;
	}

	if (regcomp(&datareg, "<form action=\"(http://login.myspace.com/index.cfm\\?fuseaction=login.process&MyToken=([^\\\"]*))\" method=\"post\" name=\"theForm\" id=\"theForm\">", 0) != 0) {
		printf("Error while compiling pattern\n");
		exit(EXIT_FAILURE);
	}

	ret = regexec(&datareg, tmpbuf, 3, match, 0);
	if (ret != 0) {
		regerror(ret, &datareg, errbuf, 255);
		printf("Error while matching pattern (login): %s\n", errbuf);
		if (ls->ignorecache)
		{
			ls->callback(false,ls->user_data);
			return;
		}
		ls->ignorecache = true;	
		free(tmpbuf);
		Request *req = blog->browser_curl(blog->b,"http://www.myspace.com");
		blog->getfile(req,CACHE_DIR DIRSEP "login", ls->ignorecache, NULL, parse_login, ls);
		return;
	}
	tmpbuf[match[1].rm_eo] = '\0';
	p->token = strdup(&tmpbuf[match[2].rm_so]);
	url = strdup(&tmpbuf[match[1].rm_so]);
	printf("token is %s\n",p->token);
	free(tmpbuf);

	Request *req = blog->browser_curl(blog->b,url);
	blog->browser_append_post(req,
		"email",p->username,
		"password",p->password,
		"Remember","Remember",
		"ctl00%$Main%$SplashDisplay%$login%$loginbutton.x","19",
		"ctl00%$Main%$SplashDisplay%$login%$loginbutton.y","11",
		NULL);
	blog->getfile(req,CACHE_DIR DIRSEP "logged",ls->ignorecache,&ls->retcode,check_code,ls);
}

static void login(blog_state *blog, bool ignorecache, void(*callback)(bool, void *), void* data)
{
	Request *req = blog->browser_curl(blog->b,"http://www.myspace.com");
	login_struct *ls = (login_struct*)malloc(sizeof(login_struct));
	ls->callback = callback;
	ls->user_data = data;
	ls->b = blog;
	blog->getfile(req,CACHE_DIR DIRSEP "login", ignorecache, NULL, parse_login, ls);
}

static void blog_init(blog_state *blog, const char *username, const char *password, void(*callback)(bool,void*), void*data)
{
	blog->_priv = malloc(sizeof(myspace_priv));
	myspace_priv *p = (myspace_priv*)blog->_priv;

	p->username = strdup(username);
	p->password = strdup(password);
	login(blog, false, callback,data);
}

typedef struct
{
	int limit;
	long retcode;
	blog_state *blog;
	bool ignorecache;
	void (*callback)(blog_entry **, void*);
	void *user_data;
	bool handle_data;
	blog_entry **entries;
	int count;
	int true_entries;
} journal_storage;

static void do_entry(char *tmpbuf, void *data);

static void get_entries(blog_state *blog, bool ignorecache, void (*callback)(blog_entry **, void*), void *user_data)
{
	journal_storage *js = (journal_storage*)calloc(1,sizeof(journal_storage));
	js->entries = (blog_entry**)calloc(1,sizeof(blog_entry*));
	js->count = 0;
	js->true_entries = 0;
	js->blog = blog;
	js->ignorecache = ignorecache;
	js->callback = callback;
	js->user_data = user_data;
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
	js->handle_data = false;
	do_entry(NULL,js);
}

static void parse_blog(char *tmpbuf, void *data);

static void do_entry(char *tmpbuf, void *data)
{
	journal_storage *js = data;
	blog_state *blog = js->blog;
	myspace_priv *p = (myspace_priv*)js->blog->_priv;
	regmatch_t match[3];
	char *url;
	while(1)
	{
		if (js->handle_data)
		{
			regex_t datareg;
			int ret;
			char errbuf[255];
			if (regcomp(&datareg, "(http://blog.myspace.com/index.cfm\\?fuseaction=blog.ListAll&amp;friendID=\\d+)", 0) != 0) {
				printf("Error while compiling pattern\n");
				exit(EXIT_FAILURE);
			}
			ret = regexec(&datareg, tmpbuf, 2, match, 0);
			if (ret != 0) {
				regerror(ret, &datareg, errbuf, 255);
				printf("Error while matching pattern (blog): %s\n", errbuf);
				/*if (js->ignorecache)
					exit(EXIT_FAILURE);*/
				if (strstr(tmpbuf,"Object moved to <a href=\"http://login.myspace.com/")!=NULL)
				{
					js->ignorecache = true;
					login(js->blog, js->ignorecache, entry_login, js);
					return;
				}
				free(tmpbuf);
			}
			else
			{
				js->limit = 0;
				break;
			}
			js->ignorecache = true;	
			js->handle_data = false;
			//return;
		}
		if(js->limit==2)
			break;
		
		url = g_strdup_printf("http://home.myspace.com/index.cfm?fuseaction=user&MyToken=%s",p->token);
		Request *req = blog->browser_curl(js->blog->b, url);
		js->limit++;
		js->handle_data = true;
		blog->getfile(req,CACHE_DIR DIRSEP "home", js->ignorecache, &js->retcode,do_entry,js);
		free(url);
		return;
	}
	if (js->limit == 2)
	{
		printf("Much failing\n");
		exit(EXIT_FAILURE);
	}	
	
	tmpbuf[match[1].rm_eo] = '\0';
	url = strdup(&tmpbuf[match[1].rm_so]);
	free(tmpbuf);
	
	Request *req = blog->browser_curl(js->blog->b,url);
	blog->getfile(req,CACHE_DIR DIRSEP "blog", js->ignorecache, &js->retcode, parse_blog, js);
	free(url);
}

//static replace space[] = {{" ","%20"}};

static void parse_blog(char *tmpbuf, void *data)
{
	journal_storage *js = data;
	//blog_state *blog = js->blog;
	regex_t hrefreg,moodreg,datareg,nextreg;
	int ret;
	regmatch_t match[3];
	
	if (js->retcode == 400)
	{
		free(js->entries);
		printf("tmpbuf:%s\n",tmpbuf);
		exit(1);
		js->count = 0;
		js->entries = (blog_entry**)calloc(1,sizeof(blog_entry*));
		js->ignorecache = true;
		login(js->blog, true, entry_login, js);
		return;
	}

	if (regcomp(&nextreg, "(http://blog.myspace.com/index.cfm\\?fuseaction=blog.listAll&friendID=\\d+&startID=\\d+&StartPostedDate=\\d+-\\d+-\\d+ \\d+:\\d+:\\d+&next=\\d+&page=\\d+&Mytoken=[^\"]+)\">Older", REG_DOTALL|REG_NEWLINE) != 0) {
		printf("Error while compiling pattern\n");
		exit(EXIT_FAILURE);
	}

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
	{
		printf("No entries!\n");
		js->entries[js->count] = NULL;
	}
	else
	{
		uint8_t skip = 0;
		js->entries = (blog_entry**)realloc(js->entries,(js->count+2)*sizeof(blog_entry*));
		js->entries[js->count] = (blog_entry*)malloc(sizeof(blog_entry));
		while (ret==0)
		{
			struct tm out;
			const char * cat = &tmpbuf[index+match[1].rm_so];
			char * content = &tmpbuf[index+match[2].rm_so];
			tmpbuf[index+match[1].rm_eo] = '\0';
			tmpbuf[index+match[2].rm_eo] = '\0';
			if (skip == 0 && strcmp(cat,"TimeStamp")==0)
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
				int end,i;
				regmatch_t mood[4];
				ret = regexec(&moodreg, content, 4, mood, 0);
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
				while (content[end]==' ' || content[end]=='\r' || content[end]=='\t'|| content[end]=='\n')
					end--;
				content[end+1]='\0';
				if (skip>0)
					skip--;
				js->true_entries++;
				if (skip==0)
				{
					for (i=0;i<js->count;i++)
					{
						if (strcmp(js->entries[i]->title,content)==0)
							break;
					}
					if (i==js->count)
					{
						js->entries = (blog_entry**)realloc(js->entries,(js->count+2)*sizeof(blog_entry*));
						js->entries[js->count] = (blog_entry*)malloc(sizeof(blog_entry));
						js->entries[js->count]->title = strdup(content);
						printf("Subject(%d): %s\n",js->count,content);
					}
					else
						skip++;
				}
			}
			else if (skip ==0 && strcmp(cat,"ContentInfo")==0)
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
						printf("Time(%d) = %s (from %s)\n",js->count,outstr,name);
						memcpy(&(js->entries[js->count]->date),&out,sizeof(struct tm));
						js->count++;
					}
					/*else
						printf("url=%s, name=%s\n",url,name);*/
						
					c_index += link[0].rm_eo+1;
					ret = regexec(&hrefreg, &content[c_index], 3, link, 0);
				}
				printf("\n");
			}
			else if (skip == 0 && strcmp(cat,"Content")==0)
				js->entries[js->count]->content = strdup(content);
			else if (skip == 0)
				printf("cat=%s, content = %s\n",cat, content);

			index += match[0].rm_eo+1;
			//printf("dist = %d (%9s)\n",match[0].rm_eo,&tmpbuf[index]);
			ret = regexec(&datareg, &tmpbuf[index], 3, match, 0);
		}
		js->entries[js->count] = NULL;
		ret = regexec(&nextreg, tmpbuf, 2, match, 0);
		/*if (ret==0)
		{
			printf("older entries\n");
			tmpbuf[match[1].rm_eo] = '\0';
			printf("url = %s\n",&tmpbuf[match[1].rm_so]);

			char *url = findandreplace(&tmpbuf[match[1].rm_so],space,1);
			Request *req = blog->browser_curl(js->blog->b,url);
			char *name = g_strdup_printf(CACHE_DIR DIRSEP "blog-%d",js->true_entries);
			blog->getfile(req,name, js->ignorecache, &js->retcode, parse_blog, js);
			return;
		}*/
	}
	js->callback(js->entries,js->user_data);
}

static void cleanup(blog_state *blog)
{
	blog->browser_free(blog->b);
}

typedef struct
{
	blog_state *blog;
	const blog_entry* entry;
	void (*callback)(bool);
} post_data;

static void publish(char *tmpbuf, void *data);

static void blog_post(blog_state *blog, const blog_entry* entry, void (*callback)(bool))
{
	myspace_priv *p = (myspace_priv*)blog->_priv;
	post_data * pd = (post_data*)malloc(sizeof(post_data));
	pd->blog = blog;
	pd->entry = entry;
	pd->callback = callback;

	char * url;
	Request *req;
	
	url = g_strdup_printf("http://blog.myspace.com/index.cfm?fuseaction=blog.previewBlog&Mytoken=%s",p->token);
	req = blog->browser_curl(blog->b,url);

	blog->browser_append_post(req,
		"blogID","-1",
		"postTimeMarker", entry->date.tm_hour>=12?"PM":"AM",
		"subject", pd->entry->title,
		"BlogCategoryID", "0",
		"editor","false",
		"body", pd->entry->content,
		"CurrentlyASIN","",
		"CurrentlyProductName","",
		"CurrentlyProductBy","",
		"CurrentlyImageURL","",
		"CurrentlyProductURL","",
		"CurrentProductReleaseDate","",
		"CurrentlyProductType","",
		"Mode","music",
		"MoodSetID","7",
		"MoodID","0",
		"MoodOther","",
		"BlogViewingPrivacyID","0",
		"Enclosure","",
		NULL);
	blog->browser_append_post_int(req,
		"postMonth", entry->date.tm_mon+1,
		"postDay", entry->date.tm_mday,
		"postYear",entry->date.tm_year+1900,
		"postHour", entry->date.tm_hour%12,
		"postMinute", entry->date.tm_min,
		NULL);
		
	blog->getfile(req,CACHE_DIR DIRSEP "post", true, NULL, publish, pd);
	free(url);
}

static void end_post(char *tmpbuf, void *data);

static void publish(char *tmpbuf, void *data)
{
	post_data *pd = data;
	blog_state *blog = pd->blog;
	regex_t datareg;
	const blog_entry* entry = pd->entry;
	regmatch_t match[2];
	int ret;

	if (regcomp(&datareg, "<input type=\"hidden\" name=\"hash\" value=\"([^\"]*)", 0) != 0) {
		printf("Error while compiling pattern\n");
		exit(EXIT_FAILURE);
	}
	ret = regexec(&datareg, tmpbuf, 2, match, 0);
	if (ret != 0)
	{
		printf("Panic! No hash!\n");
		pd->callback(false);
	}
	tmpbuf[match[1].rm_eo] = '\0';
	Request *req = blog->browser_curl(pd->blog->b,"http://blog.myspace.com/index.cfm?fuseaction=blog.processCreate");
	
	blog->browser_append_post(req,
		"blogID","-1",
		"postTimeMarker", entry->date.tm_hour>=12?"PM":"AM",
		"subject", pd->entry->title,
		"BlogCategoryID", "0",
		"editor","false",
		"body", pd->entry->content,
		"CurrentlyASIN","",
		"CurrentlyProductName","",
		"CurrentlyProductBy","",
		"CurrentlyImageURL","",
		"CurrentlyProductURL","",
		"CurrentProductReleaseDate","",
		"CurrentlyProductType","",
		"Mode","music",
		"MoodSetID","7",
		"MoodID","0",
		"MoodOther","",
		"BlogViewingPrivacyID","0",
		"hash", &tmpbuf[match[1].rm_so],
		"Enclosure","",
		NULL);
	blog->browser_append_post_int(req,
		"postMonth", entry->date.tm_mon+1,
		"postDay", entry->date.tm_mday,
		"postYear",entry->date.tm_year+1900,
		"postHour", entry->date.tm_hour%12,
		"postMinute", entry->date.tm_min,
		NULL);
	free(tmpbuf);
	blog->getfile(req,CACHE_DIR DIRSEP "posted", true, NULL, end_post, pd);
}

static void end_post(char *tmpbuf, void *data)
{
	post_data *pd = data;
	free(tmpbuf);
		
	free(pd);
	pd->callback(true);
}

__BEGIN_DECLS
const blog_system functions = {"myspace.png","http://www.myspace.com",blog_init,get_entries,blog_post, cleanup};
const char *blog_name(){return "MySpace";}
__END_DECLS
