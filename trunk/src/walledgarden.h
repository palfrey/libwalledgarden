#ifndef BLOG_H
#define BLOG_H 1

#include <stdbool.h>
#include <time.h>
#include <glib.h>

typedef struct _browser browser;
typedef struct _Request Request;

typedef struct _blog_state
{
	browser *b;
	void *_priv;
	browser* (* browser_init)(const char *jar);
	void (*browser_free)(browser* b);
	Request* (*browser_curl)(browser *b, const char *url);
	void (*browser_append_post)(Request *req, ...);
	void (*browser_append_post_int)(Request *req, ...);
	void (*browser_set_referer)(Request *req, const char* referer);
	void (*getfile)(Request *req, const char *filename, bool ignorefile, long *retcode, void (*callback)(char *, void *data), void *data);
} blog_state;

typedef struct _blog_entry
{
	char *title;
	char *content;
	struct tm date;
} blog_entry;

typedef struct _blog_system
{
	const char* icon;
	const char* url;
	void (*login)(blog_state *blog, const char *username, const char *password, void(*callback)(bool,void*),void *data);
	void (*entries)(blog_state *blog, bool ignorecache, void (*callback)(blog_entry **, void *data),void *data);
	void (*post)(blog_state *,const blog_entry*, void (*callback)(bool));
	void (*cleanup)(blog_state *blog);
} blog_system;

void walledgarden_init();

void blog_state_init(blog_state *blog);
const blog_system* blog_choose(const char *name);

GSList* blog_systems();

#endif
