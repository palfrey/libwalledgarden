/* This file is derived from src/cookies.h in the wget source, and
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

#ifndef COOKIES_H
#define COOKIES_H

#include <stdbool.h>

struct cookie {
  char *domain;			/* domain of the cookie */
  int port;			/* port number */
  char *path;			/* path prefix of the cookie */

  unsigned discard_requested :1; /* whether cookie was created to
				   request discarding another
				   cookie. */

  unsigned secure :1;		/* whether cookie should be
				   transmitted over non-https
				   connections. */
  unsigned domain_exact :1;	/* whether DOMAIN must match as a
				   whole. */

  unsigned permanent :1;	/* whether the cookie should outlive
				   the session. */

  char *attr;			/* cookie attribute name */
  char *value;			/* cookie attribute value */

  struct cookie *next;		/* used for chaining of cookies in the
				   same domain. */
};

struct cookie_jar {
  struct cookie * cook;
  int count;		/* number of cookies in the jar. */
};
void cookie_jar_load (struct cookie_jar *jar, const char *file);
bool has_cookie(struct cookie_jar *jar,const char *name);

#endif /* COOKIES_H */

