/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2013 Red Hat, Inc.
 *  Copyright © 2019 Jan-Michael Brummer
 *  Copyright © 2019 Purism SPC
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

#include "ephy-cookies-view.h"

#define ROWS_BATCH_SIZE 15

struct _EphyCookiesView {
  EphyDataView parent_instance;

  GtkWidget *cookies_listbox;
  gint remaining_batch_size;
  guint add_rows_source_id;

  GActionGroup *action_group;

  WebKitWebsiteDataManager *data_manager;
  GList *sites_data_list;
  GList *sites_data_list_iter;
};

G_DEFINE_TYPE (EphyCookiesView, ephy_cookies_view, EPHY_TYPE_DATA_VIEW)

static gboolean add_cookie_rows_source (EphyCookiesView *self);

static void
clear_listbox (GtkWidget *listbox)
{
  GList *children, *iter;

  children = gtk_container_get_children (GTK_CONTAINER (listbox));

  for (iter = children; iter != NULL; iter = g_list_next (iter))
    gtk_widget_destroy (GTK_WIDGET (iter->data));

  g_list_free (children);
}

static guint
get_num_current_rows (EphyCookiesView *self)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (self->cookies_listbox));
  GList *iter;
  guint num_rows = 0;

  for (iter = children; iter != NULL; iter = g_list_next (iter))
    num_rows++;

  g_list_free (children);

  return num_rows;
}

static void
forget_all (GSimpleAction *action,
            GVariant      *parameter,
            gpointer       user_data)
{
  EphyCookiesView *self = EPHY_COOKIES_VIEW (user_data);

  clear_listbox (self->cookies_listbox);
  g_list_free_full (self->sites_data_list, (GDestroyNotify)webkit_website_data_unref);
  webkit_website_data_manager_clear (self->data_manager, WEBKIT_WEBSITE_DATA_COOKIES, 0, NULL, NULL, NULL);

  self->sites_data_list = NULL;
  self->sites_data_list_iter = NULL;

  ephy_data_view_set_has_search_results (EPHY_DATA_VIEW (self), TRUE);
  ephy_data_view_set_has_data (EPHY_DATA_VIEW (self), FALSE);
}

static void
forget_clicked (GtkButton *button,
                gpointer   user_data)
{
  EphyCookiesView *self = EPHY_COOKIES_VIEW (user_data);
  GtkListBoxRow *row = g_object_get_data (G_OBJECT (button), "row");
  GList *data_to_remove = g_object_get_data (G_OBJECT (row), "list-iter");

  /* We do this check to ensure our internal list iterator doesn't become invalid */
  if (self->sites_data_list_iter == data_to_remove)
    self->sites_data_list_iter = g_list_next (self->sites_data_list_iter);

  /* First we unlink the element from our internal list */
  self->sites_data_list = g_list_remove_link (self->sites_data_list, data_to_remove);

  /* Then tell WebKit to remove the site's cookies */
  webkit_website_data_manager_remove (self->data_manager, WEBKIT_WEBSITE_DATA_COOKIES, data_to_remove, NULL, NULL, NULL);

  /* Schedule the loading of another row to replace the one being removed
   * We do this to prevent the list from becoming smaller than the dialog,
   * which causes improper behavior */
  self->remaining_batch_size += 1;
  if (self->add_rows_source_id == 0)
    self->add_rows_source_id = g_idle_add ((GSourceFunc)add_cookie_rows_source, self);

  /* Finally free the unlinked element and remove the row */
  g_list_free_full (data_to_remove, (GDestroyNotify)webkit_website_data_unref);
  gtk_container_remove (GTK_CONTAINER (self->cookies_listbox), GTK_WIDGET (row));
}

static GActionGroup *
create_action_group (EphyCookiesView *self)
{
  const GActionEntry entries[] = {
    { "forget-all", forget_all }
  };

  GSimpleActionGroup *group;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), self);

  return G_ACTION_GROUP (group);
}

static void
on_search_text_changed (EphyCookiesView *self)
{
  /* Reset list state and iterator */
  clear_listbox (self->cookies_listbox);
  self->sites_data_list_iter = self->sites_data_list;
  ephy_data_view_set_is_loading (EPHY_DATA_VIEW (self), TRUE);

  /* Start loading a new batch of rows */
  self->remaining_batch_size = ROWS_BATCH_SIZE;

  if (self->add_rows_source_id == 0)
    self->add_rows_source_id = g_idle_add ((GSourceFunc)add_cookie_rows_source, self);
}

static void
on_edge_reached (GtkWidget         *scrolled,
                 GtkPositionType    pos,
                 gpointer           user_data)
{
  EphyCookiesView *self = EPHY_COOKIES_VIEW (scrolled);

  if (pos == GTK_POS_BOTTOM) {
    self->remaining_batch_size = ROWS_BATCH_SIZE;

    if (self->add_rows_source_id == 0)
      self->add_rows_source_id = g_idle_add ((GSourceFunc)add_cookie_rows_source, self);
  }
}

static void
add_cookie_row (EphyCookiesView *self,
                GList           *list_iter)
{
  GtkWidget *row;
  GtkWidget *button;
  WebKitWebsiteData *site_data = list_iter->data;
  const char *domain = webkit_website_data_get_name (site_data);

  /* Row */
  row = hdy_action_row_new ();
  hdy_action_row_set_title (HDY_ACTION_ROW (row), domain);

  button = gtk_button_new_from_icon_name ("user-trash-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  g_object_set_data (G_OBJECT (button), "row", row);
  gtk_widget_set_tooltip_text (button, _("Remove cookie"));
  g_signal_connect (button, "clicked", G_CALLBACK (forget_clicked), self);
  gtk_container_add (GTK_CONTAINER (row), button);
  g_object_set_data (G_OBJECT (row), "list-iter", list_iter);

  gtk_widget_show_all (GTK_WIDGET (row));
  gtk_list_box_insert (GTK_LIST_BOX (self->cookies_listbox), GTK_WIDGET (row), -1);
}

static gboolean
add_cookie_rows_source (EphyCookiesView *self)
{
  gint remaining_batch_size = self->remaining_batch_size;
  GList *data_list_iter = self->sites_data_list_iter;
  const gchar *search_text = ephy_data_view_get_search_text (EPHY_DATA_VIEW (self));
  gboolean fits_search = TRUE;
  WebKitWebsiteData *site_data;
  const char *site_domain;

  if (remaining_batch_size == 0) {
    self->add_rows_source_id = 0;
    return G_SOURCE_REMOVE;
  }

  if (data_list_iter == NULL) {
    if (search_text && get_num_current_rows (self) == 0) {
      /* There weren't any sites fitting the search terms */
      ephy_data_view_set_is_loading (EPHY_DATA_VIEW (self), FALSE);
      ephy_data_view_set_has_search_results (EPHY_DATA_VIEW (self), FALSE);
    }

    self->remaining_batch_size = 0;
    self->add_rows_source_id = 0;
    return G_SOURCE_REMOVE;
  }

  /* data_list_iter is not NULL, so we can process it */
  site_data = data_list_iter->data;
  site_domain = webkit_website_data_get_name (site_data);

  if (search_text)
    fits_search = !!strstr (site_domain, search_text);

  if (fits_search == FALSE) {
    /* Site domain doesn't fit search terms => skip it and move on */
    self->sites_data_list_iter = g_list_next (data_list_iter);
    return G_SOURCE_CONTINUE;
  }

  /* Site domain fits search terms => add a row for it in the listbox */
  ephy_data_view_set_has_search_results (EPHY_DATA_VIEW (self), TRUE);
  ephy_data_view_set_is_loading (EPHY_DATA_VIEW (self), FALSE);
  add_cookie_row (self, data_list_iter);

  self->remaining_batch_size--;
  self->sites_data_list_iter = g_list_next (data_list_iter);

  return G_SOURCE_CONTINUE;
}

static void
sites_data_fetched_cb (WebKitWebsiteDataManager *data_manager,
                       GAsyncResult             *result,
                       EphyCookiesView          *self)
{
  GList *data_list = webkit_website_data_manager_fetch_finish (data_manager, result, NULL);

  self->sites_data_list = data_list;
  if (!data_list) {
    ephy_data_view_set_has_data (EPHY_DATA_VIEW (self), FALSE);
    ephy_data_view_set_is_loading (EPHY_DATA_VIEW (self), FALSE);

    return;
  }

  ephy_data_view_set_has_data (EPHY_DATA_VIEW (self), TRUE);
  ephy_data_view_set_is_loading (EPHY_DATA_VIEW (self), FALSE);

  /* Begin creating the listbox rows in batches */
  self->remaining_batch_size = ROWS_BATCH_SIZE;
  self->sites_data_list_iter = data_list;
  self->add_rows_source_id = g_idle_add ((GSourceFunc)add_cookie_rows_source, self);
}

static void
fetch_sites_data_async (EphyCookiesView *self)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  WebKitWebContext *web_context = ephy_embed_shell_get_web_context (shell);

  self->data_manager = webkit_web_context_get_website_data_manager (web_context);

  webkit_website_data_manager_fetch (self->data_manager,
                                     WEBKIT_WEBSITE_DATA_COOKIES,
                                     NULL,
                                     (GAsyncReadyCallback)sites_data_fetched_cb,
                                     self);
}

static void
ephy_cookies_view_init (EphyCookiesView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->action_group = create_action_group (self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "cookies", self->action_group);

  ephy_data_view_set_is_loading (EPHY_DATA_VIEW (self), TRUE);
  fetch_sites_data_async (self);
}

static void
ephy_cookies_view_class_init (EphyCookiesViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (WEBKIT_TYPE_WEBSITE_DATA);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/cookies-view.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyCookiesView, cookies_listbox);

  gtk_widget_class_bind_template_callback (widget_class, on_search_text_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_edge_reached);
}
