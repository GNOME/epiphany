/*
 *  Copyright © 2002  Ricardo Fernández Pascual
 *  Copyright © 2003, 2004  Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005  Christian Persch
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>

#include "ephy-security-levels.h"

G_BEGIN_DECLS

#define EPHY_TYPE_LOCATION_ENTRY (ephy_location_entry_get_type())

G_DECLARE_FINAL_TYPE (EphyLocationEntry, ephy_location_entry, EPHY, LOCATION_ENTRY, GtkEntry)

GtkWidget      *ephy_location_entry_new			(void);

void		ephy_location_entry_set_completion	(EphyLocationEntry *entry,
							 GtkTreeModel *model,
							 guint text_col,
							 guint action_col,
							 guint keywords_col,
							 guint relevance_col,
							 guint url_col,
							 guint extra_col,
							 guint favicon_col);

void		ephy_location_entry_set_location	(EphyLocationEntry *entry,
							 const char *address);

void		ephy_location_entry_set_match_func	(EphyLocationEntry *entry, 
							 GtkEntryCompletionMatchFunc match_func,
							 gpointer user_data,
							 GDestroyNotify notify);
					
const char     *ephy_location_entry_get_location	(EphyLocationEntry *entry);

gboolean	ephy_location_entry_get_can_undo	(EphyLocationEntry *entry);

gboolean	ephy_location_entry_get_can_redo	(EphyLocationEntry *entry);

GSList         *ephy_location_entry_get_search_terms	(EphyLocationEntry *entry);

gboolean	ephy_location_entry_reset		(EphyLocationEntry *entry);

void		ephy_location_entry_undo_reset		(EphyLocationEntry *entry);

void		ephy_location_entry_activate		(EphyLocationEntry *entry);

void		ephy_location_entry_set_favicon		(EphyLocationEntry *entry,
							 GdkPixbuf *pixbuf);

void            ephy_location_entry_set_show_favicon    (EphyLocationEntry *entry,
							 gboolean show_favicon);

void		ephy_location_entry_set_security_level	(EphyLocationEntry *entry,
							 EphySecurityLevel security_level);

void		ephy_location_entry_set_lock_tooltip	(EphyLocationEntry *entry,
							 const char *tooltip);

G_END_DECLS
