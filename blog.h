#ifndef BLOG_H
#define BLOG_H 1

#include "browser.h"
#include <stdbool.h>
#include <time.h>

typedef struct _blog_state
{
	browser *b;
	void *_priv;
} blog_state;

typedef struct _blog_entry
{
	char *title;
	char *content;
	struct tm date;
} blog_entry;

typedef struct _blog_system
{
	bool (*init)(blog_state *blog, const char *username, const char *password);
	blog_entry ** (*entries)(blog_state *blog, bool ignorecache);
	bool (*post)(blog_state *,const blog_entry*);
	void (*cleanup)(blog_state *blog);
} blog_system;

const blog_system* blog_choose(const char *name);

#endif
