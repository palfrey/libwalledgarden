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
	void (*init)(blog_state *blog, const char *username, const char *password, void(*callback)(bool,void*));
	void (*entries)(blog_state *blog, bool ignorecache, void (*callback)(blog_entry **));
	void (*post)(blog_state *,const blog_entry*, void (*callback)(bool));
	void (*cleanup)(blog_state *blog);
} blog_system;

void walledgarden_init();

const blog_system* blog_choose(const char *name);

#endif
