/*
 *  Copyright (C) 2002 Jorn Baayen
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
 */

#include <glib-object.h>
#include <gtk/gtkimage.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "ephy-favicon-cache.h"

#ifndef __EPHY_FAVICON_H
#define __EPHY_FAVICON_H

G_BEGIN_DECLS

#define EPHY_TYPE_FAVICON         (ephy_favicon_get_type ())
#define EPHY_FAVICON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_FAVICON, EphyFavicon))
#define EPHY_FAVICON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), EPHY_TYPE_FAVICON, EphyFaviconClass))
#define EPHY_IS_FAVICON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_FAVICON))
#define EPHY_IS_FAVICON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_FAVICON))
#define EPHY_FAVICON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_FAVICON, EphyFaviconClass))

typedef struct EphyFaviconPrivate EphyFaviconPrivate;

typedef struct
{
	GtkImage parent;

	EphyFaviconPrivate *priv;
} EphyFavicon;

typedef struct
{
	GtkImageClass parent_class;

	void (*changed) (EphyFavicon *favicon);
} EphyFaviconClass;

GType       ephy_favicon_get_type (void);

GtkWidget  *ephy_favicon_new      (const char *url);

void        ephy_favicon_set_url  (EphyFavicon *favicon,
				   const char *url);

const char *ephy_favicon_get_url  (EphyFavicon *favicon);

G_END_DECLS

#endif /* __EPHY_FAVICON_H */
