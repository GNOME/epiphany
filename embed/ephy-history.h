/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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

#ifndef EPHY_HISTORY_H
#define EPHY_HISTORY_H

#include <glib-object.h>

#include "ephy-node.h"

G_BEGIN_DECLS

typedef struct EphyHistoryClass EphyHistoryClass;

#define EPHY_HISTORY_TYPE             (ephy_history_get_type ())
#define EPHY_HISTORY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_HISTORY_TYPE, EphyHistory))
#define EPHY_HISTORY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_HISTORY_TYPE, EphyHistoryClass))
#define IS_EPHY_HISTORY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_HISTORY_TYPE))
#define IS_EPHY_HISTORY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_HISTORY_TYPE))

typedef struct EphyHistory EphyHistory;
typedef struct EphyHistoryPrivate EphyHistoryPrivate;

enum
{
	EPHY_NODE_PAGE_PROP_TITLE = 2,
	EPHY_NODE_PAGE_PROP_LOCATION = 3,
	EPHY_NODE_PAGE_PROP_VISITS = 4,
	EPHY_NODE_PAGE_PROP_LAST_VISIT = 5,
	EPHY_NODE_PAGE_PROP_FIRST_VISIT = 6,
	EPHY_NODE_PAGE_PROP_HOST_ID = 7,
	EPHY_NODE_PAGE_PROP_PRIORITY = 8,
	EPHY_NODE_PAGE_PROP_ICON = 9
};

struct EphyHistory
{
        GObject parent;
        EphyHistoryPrivate *priv;
};

struct EphyHistoryClass
{
        GObjectClass parent_class;

	void (* visited) (const char *url);
};

GType		ephy_history_get_type		(void);

EphyHistory    *ephy_history_new		(void);

EphyNode       *ephy_history_get_hosts          (EphyHistory *gh);

EphyNode       *ephy_history_get_pages          (EphyHistory *gh);

EphyNode       *ephy_history_get_page           (EphyHistory *gh,
						 const char *url);

void            ephy_history_add_page           (EphyHistory *gh,
						 const char *url);

gboolean        ephy_history_is_page_visited    (EphyHistory *gh,
						 const char *url);

int             ephy_history_get_page_visits    (EphyHistory *gh,
						 const char *url);

void            ephy_history_set_page_title     (EphyHistory *gh,
						 const char *url,
						 const char *title);

const char     *ephy_history_get_last_page	(EphyHistory *gh);

void		ephy_history_set_icon           (EphyHistory *gh,
						 const char *url,
						 const char *icon);

void            ephy_history_remove             (EphyHistory *gh,
						 EphyNode *node);

void            ephy_history_clear              (EphyHistory *gh);

G_END_DECLS

#endif
