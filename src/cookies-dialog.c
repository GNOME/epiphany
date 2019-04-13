/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2013 Red Hat, Inc.
 *  Copyright © 2019 Jan-Michael Brummer
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#include <libsoup/soup.h>
#include <webkit2/webkit2.h>

#include "ephy-string.h"
#include "ephy-shell.h"

#include "cookies-dialog.h"

struct _EphyCookiesDialog {
  GtkDialog parent_instance;

  GtkWidget *cookies_listbox;
  GtkWidget *search_bar;
  GtkWidget *search_entry;

  GActionGroup *action_group;

  WebKitWebsiteDataManager *data_manager;
  gboolean filled;

  char *search_text;
};

G_DEFINE_TYPE (EphyCookiesDialog, ephy_cookies_dialog, GTK_TYPE_DIALOG)

static void populate_model (EphyCookiesDialog *self);

static void
clear_listbox (GtkWidget *listbox)
{
  GList *children, *iter;

  children = gtk_container_get_children (GTK_CONTAINER (listbox));

  for (iter = children; iter != NULL; iter = g_list_next (iter)) {
    gtk_widget_destroy (GTK_WIDGET (iter->data));
  }

  g_list_free (children);
}

static void
reload_model (EphyCookiesDialog *self)
{
  clear_listbox (self->cookies_listbox);
  self->filled = FALSE;
  populate_model (self);
}

static void
ephy_cookies_dialog_dispose (GObject *object)
{
  EphyCookiesDialog *self = EPHY_COOKIES_DIALOG (object);

  g_clear_pointer (&self->search_text, g_free);
  G_OBJECT_CLASS (ephy_cookies_dialog_parent_class)->dispose (object);
}

static void
ephy_cookies_dialog_finalize (GObject *object)
{
  g_free (EPHY_COOKIES_DIALOG (object)->search_text);
  G_OBJECT_CLASS (ephy_cookies_dialog_parent_class)->finalize (object);
}

static void
forget_clicked (GtkButton *button,
                gpointer   user_data)
{
  EphyCookiesDialog *self = EPHY_COOKIES_DIALOG (user_data);
  GtkListBoxRow *row = g_object_get_data (G_OBJECT (button), "row");
  GList *data_to_remove = NULL;
  WebKitWebsiteData *data = NULL;

  gtk_list_box_select_row (GTK_LIST_BOX (self->cookies_listbox), row);
  data = g_object_get_data (G_OBJECT (row), "data");
  data_to_remove = g_list_append (data_to_remove, data);

  if (data_to_remove) {
    webkit_website_data_manager_remove (self->data_manager, WEBKIT_WEBSITE_DATA_COOKIES, data_to_remove, NULL, NULL, NULL);
    g_list_free_full (data_to_remove, (GDestroyNotify)webkit_website_data_unref);

    gtk_container_remove (GTK_CONTAINER (self->cookies_listbox), GTK_WIDGET (row));
  }
}

static void
on_search_entry_changed (GtkSearchEntry    *entry,
                         EphyCookiesDialog *self)
{
  const char *text;

  text = gtk_entry_get_text (GTK_ENTRY (entry));
  g_free (self->search_text);
  self->search_text = g_strdup (text);

  gtk_list_box_invalidate_filter (GTK_LIST_BOX (self->cookies_listbox));
}

static void
forget_all (GSimpleAction *action,
            GVariant      *parameter,
            gpointer       user_data)
{
  EphyCookiesDialog *self = EPHY_COOKIES_DIALOG (user_data);

  webkit_website_data_manager_clear (self->data_manager, WEBKIT_WEBSITE_DATA_COOKIES, 0, NULL, NULL, NULL);
  reload_model (self);
}

static void
ephy_cookies_dialog_class_init (EphyCookiesDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ephy_cookies_dialog_dispose;
  object_class->finalize = ephy_cookies_dialog_finalize;

  g_type_ensure (WEBKIT_TYPE_WEBSITE_DATA);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/cookies-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyCookiesDialog, cookies_listbox);
  gtk_widget_class_bind_template_child (widget_class, EphyCookiesDialog, search_bar);
  gtk_widget_class_bind_template_child (widget_class, EphyCookiesDialog, search_entry);

  gtk_widget_class_bind_template_callback (widget_class, on_search_entry_changed);
}

static void
cookie_add (EphyCookiesDialog *self,
            WebKitWebsiteData *data)
{
  HdyActionRow *row;
  GtkWidget *button;
  const char *domain;

  domain = webkit_website_data_get_name (data);

  /* Row */
  row = hdy_action_row_new ();
  hdy_action_row_set_title (row, domain);

  button = gtk_button_new_from_icon_name ("user-trash-symbolic", GTK_ICON_SIZE_BUTTON);
  g_object_set_data (G_OBJECT (button), "row", row);
  gtk_widget_set_tooltip_text (button, _("Remove cookie"));
  g_signal_connect (button, "clicked", G_CALLBACK (forget_clicked), self);
  hdy_action_row_add_action (row, button);
  g_object_set_data (G_OBJECT (row), "data", data);

  gtk_widget_show_all (GTK_WIDGET (row));
  gtk_list_box_insert (GTK_LIST_BOX (self->cookies_listbox), GTK_WIDGET (row), -1);
}

static void
get_domains_with_cookies_cb (WebKitWebsiteDataManager *data_manager,
                             GAsyncResult             *result,
                             EphyCookiesDialog        *self)
{
  GList *data_list;

  data_list = webkit_website_data_manager_fetch_finish (data_manager, result, NULL);
  if (!data_list)
    return;

  for (GList *l = data_list; l && l->data; l = g_list_next (l))
    cookie_add (self, (WebKitWebsiteData *)l->data);

  /* The list items have been consumed, so we need only to free the list. */
  g_list_free (data_list);

  self->filled = TRUE;
}

static void
populate_model (EphyCookiesDialog *self)
{
  g_assert (self->filled == FALSE);

  webkit_website_data_manager_fetch (self->data_manager,
                                     WEBKIT_WEBSITE_DATA_COOKIES,
                                     NULL,
                                     (GAsyncReadyCallback)get_domains_with_cookies_cb,
                                     self);
}

static GActionGroup *
create_action_group (EphyCookiesDialog *self)
{
  const GActionEntry entries[] = {
    { "forget-all", forget_all }
  };

  GSimpleActionGroup *group;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), self);

  return G_ACTION_GROUP (group);
}

static gboolean
filter_func (GtkListBoxRow *row,
             gpointer       user_data)
{
  EphyCookiesDialog *self = EPHY_COOKIES_DIALOG (user_data);

  if (self->search_text) {
    return strstr (hdy_action_row_get_title (HDY_ACTION_ROW (row)), self->search_text) != NULL;
  }

  return TRUE;
}

static void
ephy_cookies_dialog_init (EphyCookiesDialog *self)
{
  WebKitWebContext *web_context;
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();

  gtk_widget_init_template (GTK_WIDGET (self));

  web_context = ephy_embed_shell_get_web_context (shell);
  self->data_manager = webkit_web_context_get_website_data_manager (web_context);

  populate_model (self);

  self->action_group = create_action_group (self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "cookies", self->action_group);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->cookies_listbox), hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_filter_func (GTK_LIST_BOX (self->cookies_listbox), filter_func, self, NULL);

  hdy_search_bar_connect_entry (HDY_SEARCH_BAR (self->search_bar), GTK_ENTRY (self->search_entry));
}

EphyCookiesDialog *
ephy_cookies_dialog_new (void)
{
  return g_object_new (EPHY_TYPE_COOKIES_DIALOG, "use-header-bar", TRUE, NULL);
}
