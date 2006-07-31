#define RAPTOR_STATIC
#define _GNU_SOURCE
#include <string.h>
#include "common.h"
#include <raptor.h>
#include "blog.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <glib.h>

blog_entry submit;
blog_state blog;
char url[255];
const blog_system *bs;

char **posts = NULL;
int post_count =0;

replace items[] = {
				{"</li><br /><li>","</li><li>"},
				{"<ul><br /><li>","<ul><li>"},
				{"</li><br /></ul>","</li></ul>"}
				};

void print_triple(void* user_data, const raptor_statement* triple) 
{
	if (triple->subject_type != RAPTOR_IDENTIFIER_TYPE_ANONYMOUS)
	{
		if (strcmp(triple->predicate, "http://purl.org/rss/1.0/title")==0)
		{
			free(submit.title);
			submit.title = strdup(triple->object);
		}	
		else if (strcmp(triple->predicate, "http://purl.org/rss/1.0/link")==0)
			sprintf(url, "<i>Original post is <a href=\"%s\">here</a> on my Livejournal blog</i>\n\n",(char*)triple->object);
		else if (strcmp(triple->predicate, "http://purl.org/rss/1.0/description")==0)
		{
			char *out = findandreplace((char*)triple->object,items,sizeof(items)/sizeof(replace));
			free(submit.content);
			submit.content = g_strdup_printf("%s%s",url,out);
			free(out);
		}	
		else if (strcmp(triple->predicate, "http://purl.org/dc/elements/1.1/date")==0)
		{
			char *ret = strptime((char*)triple->object,"%Y-%m-%dT%H:%M:%SZ",&submit.date);
			if (ret == NULL || ret[0]!='\0')
			{
				printf("Ret = %s\n",ret);
				exit(1);
			}	
			char outstr[200];
			int i;
			strftime(outstr,sizeof(outstr),"%Y-%m-%d",&submit.date);
			printf("date = %s\n\n",outstr);
			for (i=0;i<post_count;i++)
			{
				if (strcmp(posts[i],submit.title)==0)
					break;
			}
			printf("blog entry:\ntitle = '%s'\ncontent = %s\n",submit.title,submit.content);
			if (i==post_count)
			{
				printf("posting!\n");
				bs->post(&blog,&submit);
			}
			free(submit.content);
			free(submit.title);
			/*if (i==post_count)
				exit(1);*/
			memset(&submit,0,sizeof(submit));
		}	
	}	
	/*else
		printf("obj:%s\n",(char*)triple->predicate);*/
	/*raptor_print_statement_as_ntriples(triple, stdout);
	fputc('\n', stdout);*/
} 

int main(int argc, char **argv)
{
	//bs = blog_choose("okcupid");
	bs = blog_choose("myspace");
	if (argc!=3)
	{
		printf("Usage: %s <username> <password>\n",argv[0]);
		exit(EXIT_FAILURE);
	}

	char *username = argv[1];
	char *password = argv[2];
	
	if (!exists(CACHE_DIR))
	{
		#ifdef _WIN32
		mkdir(CACHE_DIR);
		#else
		mkdir(CACHE_DIR, S_IRWXU);
		#endif
	}

	bs->init(&blog,username,password);
	blog_entry ** entries = bs->entries(&blog, true);
	blog_entry **curr = entries;
	while (curr!=NULL && *curr!=NULL)
	{
		char outstr[200];
		printf("title = %s\n",(*curr)->title);
		strftime(outstr,sizeof(outstr),"%Y-%m-%d",&((*curr)->date));
		printf("date = %s\n",outstr);
		printf("content = %s\n\n",(*curr)->content);

		posts = (char**)realloc(posts,(post_count+1)*sizeof(char*));
		posts[post_count] = strdup((*curr)->title);
		curr++;
		post_count++;
	}
	
	browser *b = browser_init("cookies");
	CURL *c = browser_curl(b);
	curl_easy_setopt(c,CURLOPT_URL,"http://palfrey.livejournal.com/data/rss");
	char *tmpbuf = getfile(c,CACHE_DIR DIRSEP "rss",false, NULL);

	raptor_parser* rdf_parser=NULL;
	raptor_uri *base_uri;

	raptor_init();

	rdf_parser=raptor_new_parser("rss-tag-soup");

	raptor_set_statement_handler(rdf_parser, NULL, print_triple);

	unsigned char *ustring = raptor_uri_filename_to_uri_string(CACHE_DIR DIRSEP "rss");
	base_uri=raptor_new_uri(ustring);
	free(ustring);

	raptor_start_parse(rdf_parser, base_uri);
	raptor_parse_chunk(rdf_parser, (unsigned char*)tmpbuf, strlen(tmpbuf), 1);
	raptor_free_parser(rdf_parser);

	raptor_free_uri(base_uri);

	raptor_finish();
	free(tmpbuf);

	/*blog_entry submit;
	submit.title = strdup("Testing entry");
	submit.content = strdup("This is what I'd like to have displayed in my journal.\n\nLook, paragraphs!");
	strptime("2006-06-10T00:00:00","%Y-%m-%dT%H:%M:%SZ",&submit.date);

	bs->post(&blog,&submit);*/
	bs->cleanup(&blog);
	
	return 0;
}

