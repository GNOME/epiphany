/* Nautilus - Floating status bar.
 *
 * Copyright (C) 2011 Red Hat Inc.
 * Copyright (C) 2018 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_FLOATING_BAR nautilus_floating_bar_get_type ()

G_DECLARE_FINAL_TYPE (NautilusFloatingBar, nautilus_floating_bar, NAUTILUS, FLOATING_BAR, GtkBox)

GtkWidget *nautilus_floating_bar_new               (void);

void       nautilus_floating_bar_set_primary_label (NautilusFloatingBar *self,
                                                    const char          *label);

G_END_DECLS
