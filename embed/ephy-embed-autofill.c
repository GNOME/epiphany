/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2019 Abdullah Alansari
 *  Copyright © 2024 Jan-Michael Brummer
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

#include "config.h"
#include "ephy-embed-autofill.h"

#include "ephy-autofill-fill-choice.h"
#include "ephy-autofill-storage.h"
#include "ephy-settings.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

static void
autofill_fill_button_clicked_cb (GAction     *action,
                                 GVariant    *parameter,
                                 EphyWebView *view)
{
  const char *selector;
  int choice;

  g_variant_get (parameter, "(us)", &choice, &selector);
  ephy_web_view_autofill (view, selector, choice);
}

static void
add_menu_item (const char             *label,
               GSimpleActionGroup     *action_group,
               GMenu                  *menu,
               EphyWebView            *view,
               const char             *selector,
               EphyAutofillFillChoice  fill_choice)
{
  g_autoptr (GMenuItem) menu_item = NULL;
  g_autoptr (GSimpleAction) action = NULL;
  g_autofree char *action_name = g_strdup_printf ("%d", fill_choice);
  g_autofree char *full_action_name = g_strconcat ("autofill.", action_name, NULL);

  menu_item = g_menu_item_new (label, full_action_name);
  g_menu_item_set_action_and_target_value (menu_item, full_action_name, g_variant_new ("(us)", fill_choice, selector));
  g_menu_append_item (menu, menu_item);

  action = g_simple_action_new (action_name, g_variant_type_new ("(us)"));
  g_signal_connect (action, "activate", G_CALLBACK (autofill_fill_button_clicked_cb), view);
  g_action_map_add_action (G_ACTION_MAP (action_group), G_ACTION (action));
}

static void
autofill_never_show_clicked_cb (EphyWebView *view,
                                GVariant    *parameter,
                                GAction     *action)
{
  ephy_web_view_autofill_disable_popup (view);
}

static void
popover_close (GtkWidget *webview,
               GtkWidget *popover)
{
  gtk_widget_unparent (popover);
}

void
ephy_embed_autofill_signal_received_cb (EphyEmbedShell *shell,
                                        unsigned long   page_id,
                                        const char     *css_selector,
                                        gboolean        is_fillable_element,
                                        gboolean        has_personal_fields,
                                        gboolean        has_card_fields,
                                        unsigned long   element_x,
                                        unsigned long   element_y,
                                        unsigned long   element_width,
                                        unsigned long   element_height,
                                        EphyWebView    *view)
{
  g_autoptr (GMenu) menu = NULL;
  GSimpleActionGroup *action_group = g_simple_action_group_new ();
  g_autoptr (GSimpleAction) action = NULL;
  GMenuItem *menu_item;
  GtkWidget *popover;

  if (!g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_AUTOFILL_DATA))
    return;

  if (webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)) != page_id ||
      !ephy_web_view_autofill_popup_enabled (view))
    return;

  menu = g_menu_new ();

  action = g_simple_action_new ("do-not-autofill", NULL);
  g_signal_connect_swapped (action, "activate", G_CALLBACK (autofill_never_show_clicked_cb), view);
  g_action_map_add_action (G_ACTION_MAP (action_group), G_ACTION (action));

  if (has_card_fields) {
    add_menu_item (_("Autofill All Fields"),
                   action_group,
                   menu,
                   view,
                   css_selector,
                   EPHY_AUTOFILL_FILL_CHOICE_FORM_ALL);
  }

  if (has_personal_fields) {
    add_menu_item (_("Autofill Personal Fields"),
                   action_group,
                   menu,
                   view,
                   css_selector,
                   EPHY_AUTOFILL_FILL_CHOICE_FORM_PERSONAL);
  }
  if (is_fillable_element) {
    add_menu_item (_("Fill This Field"),
                   action_group,
                   menu,
                   view,
                   css_selector,
                   EPHY_AUTOFILL_FILL_CHOICE_ELEMENT);
  }

  menu_item = g_menu_item_new (_("Do Not Autofill"), "autofill.do-not-autofill");
  g_menu_append_item (menu, menu_item);

  popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
  g_signal_connect (G_OBJECT (view), "destroy", G_CALLBACK (popover_close), popover);
  gtk_widget_insert_action_group (popover, "autofill", G_ACTION_GROUP (action_group));
  gtk_widget_set_parent (popover, GTK_WIDGET (view));
  gtk_popover_set_pointing_to (GTK_POPOVER (popover), &(const GdkRectangle){ element_x + element_width / 2, element_y + element_height, 1, 1});

  gtk_popover_popup (GTK_POPOVER (popover));
}
