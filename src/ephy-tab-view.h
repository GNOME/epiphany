/* -*- Mode: C; tab-width: 2; indent-pages-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2020 Alexander Mikhaylenko <exalm7659@gmail.com>
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "ephy-embed.h"

#include <adwaita.h>

G_BEGIN_DECLS

#define EPHY_TYPE_TAB_VIEW (ephy_tab_view_get_type())

G_DECLARE_FINAL_TYPE (EphyTabView, ephy_tab_view, EPHY, TAB_VIEW, AdwBin)

#define EPHY_GET_TAB_VIEW_FROM_ADW_TAB_VIEW(view) (EPHY_TAB_VIEW (gtk_widget_get_parent (GTK_WIDGET (view))))

typedef void (*EphyTabViewCallback) (GtkWidget *widget,
                                     gpointer   data);

EphyTabView *ephy_tab_view_new                (void);

void         ephy_tab_view_next               (EphyTabView *self);

void         ephy_tab_view_pin                (EphyTabView *self);
void         ephy_tab_view_unpin              (EphyTabView *self);

void         ephy_tab_view_close              (EphyTabView *self,
                                               GtkWidget   *widget);
void         ephy_tab_view_close_selected     (EphyTabView *self);
void         ephy_tab_view_close_left         (EphyTabView *self);
void         ephy_tab_view_close_right        (EphyTabView *self);
void         ephy_tab_view_close_other        (EphyTabView *self);

void         ephy_tab_view_foreach            (EphyTabView         *self,
                                               EphyTabViewCallback  callback,
                                               gpointer             user_data);

gint         ephy_tab_view_get_n_pages        (EphyTabView *self);
gint         ephy_tab_view_get_selected_index (EphyTabView *self);
gint         ephy_tab_view_get_page_index     (EphyTabView *self,
                                               GtkWidget   *widget);

GtkWidget   *ephy_tab_view_get_nth_page       (EphyTabView *self,
                                               gint         index);
void         ephy_tab_view_select_nth_page    (EphyTabView *self,
                                               gint         index);

gboolean     ephy_tab_view_select_page        (EphyTabView *self,
                                               GtkWidget   *widget);

GtkWidget   *ephy_tab_view_get_selected_page  (EphyTabView *self);

AdwTabView  *ephy_tab_view_get_tab_view       (EphyTabView *self);

GList        *ephy_tab_view_get_pages         (EphyTabView *self);

gboolean      ephy_tab_view_get_is_pinned     (EphyTabView *self,
                                               GtkWidget   *widget);

gint          ephy_tab_view_add_tab           (EphyTabView *self,
                                               EphyEmbed   *embed,
                                               EphyEmbed   *parent,
                                               int          position,
                                               gboolean     jump_to);

GtkWidget    *ephy_tab_view_get_current_page  (EphyTabView *self);

void          ephy_tab_view_set_tab_bar       (EphyTabView    *self,
                                               AdwTabBar      *tab_bar);
void          ephy_tab_view_set_tab_overview  (EphyTabView    *self,
                                               AdwTabOverview *tab_overview);
void          ephy_tab_view_close_all         (EphyTabView *self);

G_END_DECLS
