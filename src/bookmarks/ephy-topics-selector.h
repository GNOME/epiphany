/*
 *  Copyright (C) 2002 Marco Pesenti Gritti <mpeseng@tin.it>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifndef EPHY_TOPICS_SELECTOR_H
#define EPHY_TOPICS_SELECTOR_H

#include "ephy-bookmarks.h"

#include <gtk/gtktreeview.h>

G_BEGIN_DECLS

#define EPHY_TYPE_TOPICS_SELECTOR	  (ephy_topics_selector_get_type ())
#define EPHY_TOPICS_SELECTOR(o)		  (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_TOPICS_SELECTOR, EphyTopicsSelector))
#define EPHY_TOPICS_SELECTOR_CLASS(k)	  (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_TOPICS_SELECTOR, EphyTopicsSelectorClass))
#define EPHY_IS_TOPICS_SELECTOR(o)	  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_TOPICS_SELECTOR))
#define EPHY_IS_TOPICS_SELECTOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_TOPICS_SELECTOR))
#define EPHY_TOPICS_SELECTOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_TOPICS_SELECTOR, EphyTopicsSelectorClass))

typedef struct EphyTopicsSelectorPrivate EphyTopicsSelectorPrivate;

typedef struct
{
	GtkTreeView parent;

	/*< private >*/
	EphyTopicsSelectorPrivate *priv;
} EphyTopicsSelector;

typedef struct
{
	GtkTreeViewClass parent;
} EphyTopicsSelectorClass;

GType		     ephy_topics_selector_get_type        (void);

GtkWidget	    *ephy_topics_selector_new             (EphyBookmarks *bookmarks,
							   EphyNode *bookmark);

void		     ephy_topics_selector_set_bookmark    (EphyTopicsSelector *selector,
							   EphyNode *bookmark);

void		     ephy_topics_selector_new_topic	  (EphyTopicsSelector *selector);

void		     ephy_topics_selector_apply		  (EphyTopicsSelector *selector);


G_END_DECLS

#endif /* EPHY_TOPICS_SELECTOR_H */
