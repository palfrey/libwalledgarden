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

int main()
{
	browser *b = browser_init(COOKIE_FILE);
	struct cookie_jar *jar = (struct cookie_jar*)calloc(1,sizeof(struct cookie_jar));
	cookie_jar_load (jar, COOKIE_FILE);
	if (!exists(CACHE_DIR))
	{
		#ifdef _WIN32
		mkdir(CACHE_DIR);
		#else
		mkdir(CACHE_DIR, S_IRWXU);
		#endif
	}
	
	char * tmpbuf;
	char token[255],url[255];

	regex_t datareg,hrefreg,moodreg;
	regmatch_t match[3];
	int ret;
	char errbuf[255];

	CURL *c = browser_curl(b);
	curl_easy_setopt(c,CURLOPT_URL,"http://www.myspace.com");
	tmpbuf = getfile(c,CACHE_DIR DIRSEP "login");

	if (regcomp(&datareg, "<form action=\"(http://login.myspace.com/index.cfm\\?fuseaction=login.process&MyToken=([^\\\"]*))\" method=\"post\" name=\"theForm\" id=\"theForm\">", 0) != 0) {
		printf("Error while compiling pattern\n");
		exit(EXIT_FAILURE);
	}
	ret = regexec(&datareg, tmpbuf, 3, match, 0);
	if (ret != 0) {
		regerror(ret, &datareg, errbuf, 255);
		printf("Error while matching pattern (login): %s\n", errbuf);
		exit(EXIT_FAILURE);
	}
	tmpbuf[match[1].rm_eo] = '\0';
	strcpy(token,&tmpbuf[match[2].rm_so]);
	strcpy(url,&tmpbuf[match[1].rm_so]);
	printf("token is %s\n",token);
	free(tmpbuf);

	c = browser_curl(b);
	curl_easy_setopt(c,CURLOPT_URL,url);
	curl_easy_setopt(c,CURLOPT_POSTFIELDS,"email=myspace%40tevp.net&password=5epsilon&Remember=Remember&loginbutton.x=41&loginbutton.y=8");
	tmpbuf = getfile(c,CACHE_DIR DIRSEP "logged");
	free(tmpbuf);

	c = browser_curl(b);
	sprintf(url,"http://home.myspace.com/index.cfm?fuseaction=user&MyToken=%s",token);
	curl_easy_setopt(c,CURLOPT_URL,url);
	tmpbuf = getfile(c,CACHE_DIR DIRSEP "home");
	if (regcomp(&datareg, "(http://blog.myspace.com/[^<]*)<br />", 0) != 0) {
		printf("Error while compiling pattern\n");
		exit(EXIT_FAILURE);
	}
	ret = regexec(&datareg, tmpbuf, 2, match, 0);
	if (ret != 0) {
		regerror(ret, &datareg, errbuf, 255);
		printf("Error while matching pattern: %s\n", errbuf);
		exit(EXIT_FAILURE);
	}
	tmpbuf[match[1].rm_eo] = '\0';
	strcpy(url,&tmpbuf[match[1].rm_so]);
	free(tmpbuf);
	
	c = browser_curl(b);
	curl_easy_setopt(c,CURLOPT_URL,url);
	tmpbuf = getfile(c,CACHE_DIR DIRSEP "blog");
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
	struct tm out;
	//printf("dist = %d (%9s)\n",match[0].rm_eo,&tmpbuf[index]);
	ret = regexec(&datareg, &tmpbuf[index], 3, match, 0);
	while (ret==0)
	{
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
			printf("Time = %s (from %s)\n\n",outstr,content);
		}
		else if (strcmp(cat,"Subject")==0)
		{
			int end;
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
			//printf("c[e] = %d end=%d\n",content[end],end);	
			while (content[end]==' ' || content[end]=='\r' || content[end]=='\t'|| content[end]=='\n')
				end--;
			//printf("c[e] = %d end=%d\n",content[end],end);	
			content[end+1]='\0';
			printf("Subject: %s\n",content);
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
					printf("Time = %s (from %s)\n",outstr,name);
				}
				/*else
					printf("url=%s, name=%s\n",url,name);*/
					
				c_index += link[0].rm_eo+1;
				ret = regexec(&hrefreg, &content[c_index], 3, link, 0);
			}
			printf("\n");
		}
		else
			printf("cat=%s, content = %s\n",cat, content);

		index += match[0].rm_eo+1;
		//printf("dist = %d (%9s)\n",match[0].rm_eo,&tmpbuf[index]);
		ret = regexec(&datareg, &tmpbuf[index], 3, match, 0);
	}	

	browser_free(b);
	return 0;
}
