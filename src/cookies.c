/* This file is derived from src/cookies.c in the wget source, and
   is therefore by default covered by the GPL. Original copyright notice is below,
   but this is not a complete copy, and so most bugs will be mine not the
   original authors.

   -- Tom Parker

==========================================================

   Support for cookies.
   Copyright (C) 2001-2005 Free Software Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at
your option) any later version.

GNU Wget is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.

In addition, as a special exception, the Free Software Foundation
gives permission to link the code of its release of Wget with the
OpenSSL project's "OpenSSL" library (or with modified versions of it
that use the same license as the "OpenSSL" library), and distribute
the linked executables.  You must obey the GNU General Public License
in all respects for all of the code used other than "OpenSSL".  If you
modify this file, you may extend this exception to your version of the
file, but you are not obligated to do so.  If you do not wish to do
so, delete this exception statement from your version.  */

/* Written by Hrvoje Niksic.  Parts are loosely inspired by the
   cookie patch submitted by Tomasz Wegrzanowski.

   This implements the client-side cookie support, as specified
   (loosely) by Netscape's "preliminary specification", currently
   available at:

       http://wp.netscape.com/newsref/std/cookie_spec.html

   rfc2109 is not supported because of its incompatibilities with the
   above widely-used specification.  rfc2965 is entirely ignored,
   since popular client software doesn't implement it, and even the
   sites that do send Set-Cookie2 also emit Set-Cookie for
   compatibility.  */
   
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "cookies.h"

#define ISSPACE(c) ((c)==' '||(c)=='\t')

static int
domain_port (const char *domain_b, const char *domain_e,
	     const char **domain_e_ptr)
{
  int port = 0;
  const char *p;
  const char *colon = memchr (domain_b, ':', domain_e - domain_b);
  if (!colon)
    return 0;
  for (p = colon + 1; p < domain_e && isdigit(*p); p++)
    port = 10 * port + (*p - '0');
  if (p < domain_e)
    /* Garbage following port number. */
    return 0;
  *domain_e_ptr = colon;
  return port;
}

char *
read_whole_line (FILE *fp)
{
  int length = 0;
  int bufsize = 82;
  char *line = (char*)malloc (bufsize);

  while (fgets (line + length, bufsize - length, fp))
    {
      length += strlen (line + length);
      if (length == 0)
	/* Possible for example when reading from a binary file where
	   a line begins with \0.  */
	continue;

      if (line[length - 1] == '\n')
	break;

      /* fgets() guarantees to read the whole line, or to use up the
         space we've given it.  We can double the buffer
         unconditionally.  */
      bufsize <<= 1;
      line = (char*)realloc (line, bufsize);
    }
  if (length == 0 || ferror (fp))
    {
      free (line);
      return NULL;
    }
  if (length + 1 < bufsize)
    /* Relieve the memory from our exponential greediness.  We say
       `length + 1' because the terminating \0 is not included in
       LENGTH.  We don't need to zero-terminate the string ourselves,
       though, because fgets() does that.  */
    line = (char*)realloc (line, length + 1);
  return line;
}

#define PORT_ANY (-1)

static struct cookie *
cookie_new (void)
{
  struct cookie *cookie = (struct cookie*)calloc(1,sizeof(struct cookie));

  /* Both cookie->permanent and cookie->expiry_time are now 0.  This
     means that the cookie doesn't expire, but is only valid for this
     session (i.e. not written out to disk).  */

  cookie->port = PORT_ANY;
  return cookie;
}
#define GET_WORD(p, b, e) do {			\
  b = p;					\
  while (*p && *p != '\t')			\
    ++p;					\
  e = p;					\
  if (b == e || !*p)				\
    goto next;					\
  ++p;						\
} while (0)

static void
store_cookie (struct cookie_jar *jar, struct cookie *cookie)
{
	jar->cook = (struct cookie*	)realloc(jar->cook,(jar->count+1)*sizeof(struct cookie));
	memcpy(&(jar->cook[jar->count]),cookie,sizeof(struct cookie));
	jar->count++;
}

/* Load cookies from FILE.  */

void cookie_jar_load (struct cookie_jar *jar, const char *file)
{
  char *line;
  FILE *fp = fopen (file, "r");
  if (!fp)
    {
      return;
    }
  for (; ((line = read_whole_line (fp)) != NULL); free (line))
    {
      struct cookie *cookie;
      char *p = line;

      double expiry;
      int port;

      char *domain_b  = NULL, *domain_e  = NULL;
      char *domflag_b = NULL, *domflag_e = NULL;
      char *path_b    = NULL, *path_e    = NULL;
      char *secure_b  = NULL, *secure_e  = NULL;
      char *expires_b = NULL, *expires_e = NULL;
      char *name_b    = NULL, *name_e    = NULL;
      char *value_b   = NULL, *value_e   = NULL;

      /* Skip leading white-space. */
      while (*p && ISSPACE (*p))
	++p;
      /* Ignore empty lines.  */
      if (!*p || *p == '#')
	continue;

      GET_WORD (p, domain_b,  domain_e);
      GET_WORD (p, domflag_b, domflag_e);
      GET_WORD (p, path_b,    path_e);
      GET_WORD (p, secure_b,  secure_e);
      GET_WORD (p, expires_b, expires_e);
      GET_WORD (p, name_b,    name_e);

      /* Don't use GET_WORD for value because it ends with newline,
	 not TAB.  */
      value_b = p;
      value_e = p + strlen (p);
      if (value_e > value_b && value_e[-1] == '\n')
	--value_e;
      if (value_e > value_b && value_e[-1] == '\r')
	--value_e;
      /* Empty values are legal (I think), so don't bother checking. */

      cookie = cookie_new ();

      name_e[0]='\0';
	  cookie->attr    = strdup (name_b);
      value_e[0]='\0';
      cookie->value   = strdup (value_b);
      path_e[0]='\0';
      cookie->path    = strdup (path_b);
      cookie->secure  = strncmp (secure_b, "TRUE",strlen("TRUE"));

      /* Curl source says, quoting Andre Garcia: "flag: A TRUE/FALSE
	 value indicating if all machines within a given domain can
	 access the variable.  This value is set automatically by the
	 browser, depending on the value set for the domain."  */
      cookie->domain_exact = strncmp(domflag_b, "TRUE",strlen("TRUE"))!=0;

      /* DOMAIN needs special treatment because we might need to
	 extract the port.  */
      port = domain_port (domain_b, domain_e, (const char **)&domain_e);
      if (port)
	cookie->port = port;

      if (*domain_b == '.')
	++domain_b;		/* remove leading dot internally */
	  domain_e[0]='\0';
      cookie->domain  = strdup (domain_b);

      /* I don't like changing the line, but it's safe here.  (line is
	 malloced.)  */
      *expires_e = '\0';
      sscanf (expires_b, "%lf", &expiry);

      store_cookie (jar, cookie);

    next:
      continue;
    }
  fclose (fp);
}

bool has_cookie(struct cookie_jar *jar,const char *name)
{
	int i;
	for (i=0;i<jar->count;i++)
	{
		if (strcmp(name,jar->cook[i].attr)==0)
			return true;
	}
	return false;
}

