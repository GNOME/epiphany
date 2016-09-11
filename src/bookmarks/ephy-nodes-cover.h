/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2004 Peter Harvey <pah06@uow.edu.au>
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

#include "ephy-bookmarks.h"

G_BEGIN_DECLS

gint ephy_nodes_remove_covered (EphyNode *parent, GPtrArray *children);

gint ephy_nodes_remove_not_covered (EphyNode *parent, GPtrArray *children);

gint ephy_nodes_count_covered (EphyNode *parent, const GPtrArray *children);
    
gboolean ephy_nodes_covered (EphyNode *parent, const GPtrArray *children);

GPtrArray * ephy_nodes_get_covered (EphyNode *parent, const GPtrArray *children, GPtrArray *_covered);

GPtrArray * ephy_nodes_get_covering (const GPtrArray *parents, const GPtrArray *children,
                                     GPtrArray *_covering, GPtrArray *_uncovered, GArray *_sizes);

G_END_DECLS
