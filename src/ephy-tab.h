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

#ifndef EPHY_TAB_H
#define EPHY_TAB_H

#include "ephy-embed.h"

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct EphyTabClass EphyTabClass;

#define EPHY_TAB_TYPE             (ephy_tab_get_type ())
#define EPHY_TAB(obj)             (GTK_CHECK_CAST ((obj), EPHY_TAB_TYPE, EphyTab))
#define EPHY_TAB_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), EPHY_TAB, EphyTabClass))
#define IS_EPHY_TAB(obj)          (GTK_CHECK_TYPE ((obj), EPHY_TAB_TYPE))
#define IS_EPHY_TAB_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), EPHY_TAB))

typedef struct EphyTab EphyTab;
typedef struct EphyTabPrivate EphyTabPrivate;

typedef enum
{
	TAB_CONTROL_TITLE
} TabControlID;

typedef enum
{
	TAB_LOAD_NONE,
	TAB_LOAD_STARTED,
	TAB_LOAD_COMPLETED
} TabLoadStatus;

struct EphyTab
{
        GObject parent;
        EphyTabPrivate *priv;
};

struct EphyTabClass
{
        GObjectClass parent_class;
};

/* Include the header down here to resolve circular dependency */
#include "ephy-window.h"

GType         ephy_tab_get_type			(void);

EphyTab      *ephy_tab_new			(void);

EphyEmbed    *ephy_tab_get_embed		(EphyTab *tab);

void          ephy_tab_set_window		(EphyTab *tab,
						 EphyWindow *window);

EphyWindow   *ephy_tab_get_window		(EphyTab *tab);

void	      ephy_tab_set_is_active		(EphyTab *tab,
						 gboolean is_active);

gboolean      ephy_tab_get_is_active		(EphyTab *tab);

gboolean      ephy_tab_get_visibility           (EphyTab *tab);

TabLoadStatus ephy_tab_get_load_status		(EphyTab *tab);

int	      ephy_tab_get_load_percent		(EphyTab *tab);

const char   *ephy_tab_get_status_message	(EphyTab *tab);

const char   *ephy_tab_get_title		(EphyTab *tab);

const char   *ephy_tab_get_location             (EphyTab *tab);

const char   *ephy_tab_get_favicon_url          (EphyTab *tab);

void	      ephy_tab_set_location             (EphyTab *tab,
						 char *location);

void	      ephy_tab_get_size			(EphyTab *tab,
						 int *width,
						 int *height);

void	      ephy_tab_update_control		(EphyTab *tab,
						 TabControlID id);

EphyEmbedEvent *ephy_tab_get_event		(EphyTab *tab);

G_END_DECLS

#endif
