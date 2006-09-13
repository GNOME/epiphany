/*
 *  Copyright © 2002 Jorn Baayen
 *  Copyright © 2003-2004 Marco Pesenti Gritti
 *  Copyright © 2004, 2005 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef EPHY_FAVICON_CACHE_H
#define EPHY_FAVICON_CACHE_H

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define EPHY_TYPE_FAVICON_CACHE         (ephy_favicon_cache_get_type ())
#define EPHY_FAVICON_CACHE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_FAVICON_CACHE, EphyFaviconCache))
#define EPHY_FAVICON_CACHE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), EPHY_TYPE_FAVICON_CACHE, EphyFaviconCacheClass))
#define EPHY_IS_FAVICON_CACHE(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_FAVICON_CACHE))
#define EPHY_IS_FAVICON_CACHE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_FAVICON_CACHE))
#define EPHY_FAVICON_CACHE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_FAVICON_CACHE, EphyFaviconCacheClass))

typedef struct _EphyFaviconCacheClass	EphyFaviconCacheClass;
typedef struct _EphyFaviconCache	EphyFaviconCache;
typedef struct _EphyFaviconCachePrivate	EphyFaviconCachePrivate;

struct _EphyFaviconCache
{
	GObject parent;

	/*< private >*/
	EphyFaviconCachePrivate *priv;
};

struct _EphyFaviconCacheClass
{
	GObjectClass parent_class;

	/* Signals */
	void (*changed)	(EphyFaviconCache *cache,
			 const char *url);
};

GType		 ephy_favicon_cache_get_type	(void);

EphyFaviconCache *ephy_favicon_cache_new	(void);

GdkPixbuf	 *ephy_favicon_cache_get	(EphyFaviconCache *cache,
						 const char *url);

void		  ephy_favicon_cache_clear	(EphyFaviconCache *cache);

G_END_DECLS

#endif /* EPHY_FAVICON_CACHE_H */
