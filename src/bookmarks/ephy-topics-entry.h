/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2002 Marco Pesenti Gritti <mpeseng@tin.it>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "ephy-bookmarks.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_TOPICS_ENTRY (ephy_topics_entry_get_type ())
G_DECLARE_FINAL_TYPE (EphyTopicsEntry, ephy_topics_entry, EPHY, TOPICS_ENTRY, GtkEntry);

GtkWidget *ephy_topics_entry_new (EphyBookmarks *bookmarks, EphyNode *bookmark);

G_END_DECLS
