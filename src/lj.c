#define RAPTOR_STATIC
#define _GNU_SOURCE
#include <curl/curl.h>
#include <string.h>
#include <raptor.h>
#include "walledgarden.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <glib.h>
#include <pthread.h>

blog_entry submit;
blog_state blog;
char url[255];
const blog_system *bs;

char **posts = NULL;
int post_count =0;

typedef struct _replace
{
	const char *from;
	const char *to;
} replace;

typedef struct {
	bool ready;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
} ThreadReady;

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

void ready_done(ThreadReady *th) {
	pthread_mutex_lock(&th->mutex);
	th->ready = true;
	pthread_cond_broadcast(&th->cond);
	pthread_mutex_unlock(&th->mutex);
}

void ready_wait(ThreadReady *th) {
	pthread_mutex_lock(&th->mutex);
	while (!th->ready) {
		pthread_cond_wait(&th->cond, &th->mutex);
	}
	pthread_mutex_unlock(&th->mutex);
}


replace items[] = {
				{"</li><br /><li>","</li><li>"},
				{"<ul><br /><li>","<ul><li>"},
				{"</li><br /></ul>","</li></ul>"}
				};

ThreadReady post_done;

void end_triple(bool success)
{
	ready_done(&post_done);
}

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
			sprintf(url, "<i>Original post is <a href=\"%s\">here</a> on my Livejournal blog</i><br/><br/>",(char*)triple->object);
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
			printf("blog entry:\ntitle = '%s'\n",submit.title);
			//printf("content = %s\n",submit.content);
			if (i==post_count)
			{
				printf("posting!\n");
				/*if (i==post_count)
					exit(1);*/
				ready_init(&post_done);
				bs->post(&blog,&submit, &end_triple);
				ready_wait(&post_done);
			}
			free(submit.content);
			free(submit.title);
			if (i==post_count)
				exit(1);
			memset(&submit,0,sizeof(submit));
		}	
	}	
	/*else
		printf("obj:%s\n",(char*)triple->predicate);*/
	/*raptor_print_statement_as_ntriples(triple, stdout);
	fputc('\n', stdout);*/
} 

void run_lj(bool, void *);

int main(int argc, char **argv)
{
	if (argc!=4)
	{
		printf("Usage: %s <blog system> <username> <password>\n",argv[0]);
		exit(EXIT_FAILURE);
	}
	bs = blog_choose(argv[1]);
	if (bs == NULL)
	{
		printf("'%s' is not a valid blog system\n",argv[1]);
		exit(EXIT_FAILURE);
	}

	char *username = argv[2];
	char *password = argv[3];
	
	blog_state_init(&blog);
	bs->login(&blog,username,password, run_lj, NULL);
	return 0;
}

void use_entries(blog_entry ** entries, void *data);

void run_lj(bool success, void * junk)
{
	if (!success)
	{
		printf("panic! login failure\n");
		exit(1);
	}
	bs->entries(&blog, false, use_entries, NULL);
}

void parse_lj(char *tmpbuf, void *junk);

void use_entries(blog_entry ** entries, void *data)
{
	blog_entry **curr = entries;
	while (curr!=NULL && *curr!=NULL)
	{
		char outstr[200];
		printf("title = %s\n",(*curr)->title);
		strftime(outstr,sizeof(outstr),"%Y-%m-%d",&((*curr)->date));
		printf("date = %s\n",outstr);
		//printf("content = %s\n\n",(*curr)->content);

		posts = (char**)realloc(posts,(post_count+1)*sizeof(char*));
		posts[post_count] = strdup((*curr)->title);
		curr++;
		post_count++;
	}
	
	CURL *c = blog.browser_curl(blog.b,"http://palfrey.livejournal.com/data/rss");
	blog.getfile(c,"cache/rss",false, NULL, parse_lj, NULL);
}

void parse_lj(char *tmpbuf, void *junk)
{
	raptor_parser* rdf_parser=NULL;
	raptor_uri *base_uri;

	raptor_init();

	rdf_parser=raptor_new_parser("rss-tag-soup");

	raptor_set_statement_handler(rdf_parser, NULL, print_triple);

	unsigned char *ustring = NULL;
	ustring = raptor_uri_filename_to_uri_string("cache/rss");
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
	
	exit(0);
}

