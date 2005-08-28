/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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

#ifndef EPHY_HISTORY_H
#define EPHY_HISTORY_H

#include <glib-object.h>

#include "ephy-node.h"

G_BEGIN_DECLS

#define EPHY_TYPE_HISTORY		(ephy_history_get_type ())
#define EPHY_HISTORY(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_HISTORY, EphyHistory))
#define EPHY_HISTORY_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_HISTORY, EphyHistoryClass))
#define EPHY_IS_HISTORY(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_HISTORY))
#define EPHY_IS_HISTORY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_HISTORY))
#define EPHY_HISTORY_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_HISTORY, EphyHistoryClass))

typedef struct _EphyHistoryClass	EphyHistoryClass;
typedef struct _EphyHistory		EphyHistory;
typedef struct _EphyHistoryPrivate	EphyHistoryPrivate;

enum
{
	EPHY_NODE_PAGE_PROP_TITLE = 2,
	EPHY_NODE_PAGE_PROP_LOCATION = 3,
	EPHY_NODE_PAGE_PROP_VISITS = 4,
	EPHY_NODE_PAGE_PROP_LAST_VISIT = 5,
	EPHY_NODE_PAGE_PROP_FIRST_VISIT = 6,
	EPHY_NODE_PAGE_PROP_HOST_ID = 7,
	EPHY_NODE_PAGE_PROP_PRIORITY = 8,
	EPHY_NODE_PAGE_PROP_ICON = 9,
	EPHY_NODE_HOST_PROP_ZOOM = 10
};

struct _EphyHistory
{
	GObject parent;

	/*< private >*/
	EphyHistoryPrivate *priv;
};

struct _EphyHistoryClass
{
        GObjectClass parent_class;

	/* Signals */
	gboolean (* add_page)	(EphyHistory *history,
				 const char *url);
	void	(* visited)	(EphyHistory *history,
				 const char *url);
	void	(* cleared)	(EphyHistory *history);

	void	(* redirect)	(EphyHistory *history,
				 const char *from_uri,
				 const char *to_uri);
};

GType		ephy_history_get_type		(void);

EphyHistory    *ephy_history_new		(void);

EphyNode       *ephy_history_get_hosts          (EphyHistory *gh);

EphyNode       *ephy_history_get_host		(EphyHistory *gh,
						 const char *url);

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
const char     *ephy_history_get_icon		(EphyHistory *gh,
						 const char *url);

void            ephy_history_clear              (EphyHistory *gh);

gboolean	ephy_history_is_enabled		(EphyHistory *history);

G_END_DECLS

#endif
