#include <glib.h>
#include <ltdl.h>
#include "walledgarden.h"
#include <stdio.h>
#include <assert.h>
#include "browser.h"
#include <sys/stat.h>
#include <string.h>

GHashTable *modules=NULL;

const blog_system* blog_choose(const char *name)
{
	if (modules == NULL)
		walledgarden_init();
	lt_dlhandle lt = g_hash_table_lookup(modules, name);
	return lt_dlsym(lt,"functions");
}

static void addkeys(gpointer key, gpointer value, gpointer user_data)
{
	GSList **blogs = user_data;
	*blogs = g_slist_append(*blogs,key);
}

GSList* blog_systems()
{
	GSList *blogs = NULL;
	g_hash_table_foreach(modules,addkeys,&blogs);
	return blogs;
}

static int load_system(const char *filename, lt_ptr data)
{
	lt_dlhandle lt = lt_dlopenext (filename);
	if (lt!=NULL)
	{
		printf("module: %s\n",filename);
		char *(*name)() = (char*(*)())lt_dlsym(lt,"blog_name");
		if (strstr(filename,"libblogcore")!=NULL)
		{
			lt_dlclose(lt);
			return 0;
		}
		assert(name!=NULL);
		printf("name: '%s' handle=%p\n",name(),lt);
		g_hash_table_insert(modules, name(),lt);
	}
	return 0;
}

void walledgarden_init()
{
	modules = g_hash_table_new(g_str_hash,g_str_equal);
	lt_dlinit();
	lt_dlforeachfile(MODULESDIR ":./modules",load_system,NULL);
	if (!exists(CACHE_DIR))
	{
		#ifdef _WIN32
		mkdir(CACHE_DIR);
		#else
		mkdir(CACHE_DIR, S_IRWXU);
		#endif
	}
}
