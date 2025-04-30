/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <gabrielivascu@gnome.org>
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
#include "synced-tabs-dialog.h"

#include "ephy-desktop-utils.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-favicon-helpers.h"
#include "ephy-shell.h"

#include <json-glib/json-glib.h>

struct _SyncedTabsDialog {
  AdwWindow parent_instance;

  EphyOpenTabsManager *manager;

  WebKitFaviconDatabase *database;

  GtkTreeModel *treestore;
  GtkWidget *treeview;

  GCancellable *cancellable;
};

G_DEFINE_FINAL_TYPE (SyncedTabsDialog, synced_tabs_dialog, ADW_TYPE_WINDOW)

enum {
  ICON_COLUMN,
  TITLE_COLUMN,
  URL_COLUMN
};

enum {
  PROP_0,
  PROP_OPEN_TABS_MANAGER,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

typedef struct {
  SyncedTabsDialog *dialog;
  char *title;
  char *url;
  guint parent_index;
} PopulateRowAsyncData;

static PopulateRowAsyncData *
populate_row_async_data_new (SyncedTabsDialog *dialog,
                             const char       *title,
                             const char       *url,
                             guint             parent_index)
{
  PopulateRowAsyncData *data;

  data = g_new (PopulateRowAsyncData, 1);
  data->dialog = g_object_ref (dialog);
  data->title = g_strdup (title);
  data->url = g_strdup (url);
  data->parent_index = parent_index;

  return data;
}

static void
populate_row_async_data_free (PopulateRowAsyncData *data)
{
  g_object_unref (data->dialog);
  g_free (data->title);
  g_free (data->url);
  g_free (data);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static void
treeview_row_activated_cb (GtkTreeView       *view,
                           GtkTreePath       *path,
                           GtkTreeViewColumn *column,
                           gpointer           user_data)
{
  EphyShell *shell;
  EphyEmbed *embed;
  GtkWindow *window;
  GtkTreeModel *model;
  GtkTreeIter iter;
  char *url;
  char *path_str;

  /* No action on top-level rows. */
  if (gtk_tree_path_get_depth (path) == 1)
    return;

  /* No action on local tabs, i.e. children of first top-level row. */
  path_str = gtk_tree_path_to_string (path);
  if (g_str_has_prefix (path_str, "0:"))
    goto out;

  model = gtk_tree_view_get_model (view);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, URL_COLUMN, &url, -1);

  shell = ephy_shell_get_default ();
  window = gtk_application_get_active_window (GTK_APPLICATION (shell));
  embed = ephy_shell_new_tab (shell, EPHY_WINDOW (window),
                              NULL, EPHY_NEW_TAB_JUMP);
  ephy_web_view_load_url (ephy_embed_get_web_view (embed), url);

  g_free (url);
out:
  g_free (path_str);
}

static void
synced_tabs_dialog_favicon_loaded_cb (GObject      *source,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  WebKitFaviconDatabase *database = WEBKIT_FAVICON_DATABASE (source);
  PopulateRowAsyncData *data = (PopulateRowAsyncData *)user_data;
  g_autoptr (GdkTexture) texture = NULL;
  g_autoptr (GIcon) favicon = NULL;
  g_autoptr (GError) error = NULL;
  GtkTreeIter parent_iter;
  char *escaped_url;

  texture = webkit_favicon_database_get_favicon_finish (database, result, &error);
  if (!texture && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  favicon = ephy_favicon_get_from_texture_scaled (texture, FAVICON_SIZE, FAVICON_SIZE);

  gtk_tree_model_get_iter_first (data->dialog->treestore, &parent_iter);
  for (guint i = 0; i < data->parent_index; i++)
    gtk_tree_model_iter_next (data->dialog->treestore, &parent_iter);

  if (!favicon) {
    const char *icon_name = ephy_get_fallback_favicon_name (data->url, EPHY_FAVICON_TYPE_SHOW_MISSING_PLACEHOLDER);

    if (!icon_name)
      icon_name = "adw-tab-icon-missing-symbolic";

    favicon = g_themed_icon_new (icon_name);
  }

  escaped_url = g_markup_escape_text (data->url, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (data->dialog->treestore),
                                     NULL, &parent_iter, -1,
                                     ICON_COLUMN, favicon,
                                     TITLE_COLUMN, data->title,
                                     URL_COLUMN, escaped_url,
                                     -1);

  g_free (escaped_url);
  populate_row_async_data_free (data);
}

static void
synced_tabs_dialog_populate_from_record (SyncedTabsDialog   *dialog,
                                         EphyOpenTabsRecord *record,
                                         gboolean            is_local,
                                         guint               index)
{
  PopulateRowAsyncData *data;
  JsonArray *url_history;
  GList *tabs;
  const char *title;
  const char *url;
  g_autoptr (GIcon) icon = NULL;

  g_assert (EPHY_IS_SYNCED_TABS_DIALOG (dialog));
  g_assert (EPHY_IS_OPEN_TABS_RECORD (record));

  if (is_local)
    title = _("Local Tabs");
  else
    title = ephy_open_tabs_record_get_client_name (record);

  icon = g_themed_icon_new ("computer-symbolic");

  /* Insert top-level row. */
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (dialog->treestore),
                                     NULL, NULL, -1,
                                     ICON_COLUMN, icon,
                                     TITLE_COLUMN, title,
                                     URL_COLUMN, NULL,
                                     -1);

  tabs = ephy_open_tabs_record_get_tabs (record);
  for (GList *l = tabs; l && l->data; l = l->next) {
    title = json_object_get_string_member (l->data, "title");
    url_history = json_object_get_array_member (l->data, "urlHistory");
    url = json_array_get_string_element (url_history, 0);

    data = populate_row_async_data_new (dialog, title, url, index);
    webkit_favicon_database_get_favicon (dialog->database, url,
                                         dialog->cancellable,
                                         synced_tabs_dialog_favicon_loaded_cb,
                                         data);
  }
}
G_GNUC_END_IGNORE_DEPRECATIONS

static void
synced_tabs_dialog_populate_model (SyncedTabsDialog *dialog)
{
  EphyOpenTabsRecord *record;
  GList *remotes;
  guint index = 0;

  /* Insert local tabs. */
  record = ephy_open_tabs_manager_get_local_tabs (dialog->manager);
  synced_tabs_dialog_populate_from_record (dialog, record, TRUE, index++);

  /* Insert remote tabs. */
  remotes = ephy_open_tabs_manager_get_remote_tabs (dialog->manager);
  for (GList *l = remotes; l && l->data; l = l->next)
    synced_tabs_dialog_populate_from_record (dialog, l->data, FALSE, index++);

  g_object_unref (record);
}

static void
synced_tabs_dialog_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  SyncedTabsDialog *dialog = EPHY_SYNCED_TABS_DIALOG (object);

  switch (prop_id) {
    case PROP_OPEN_TABS_MANAGER:
      if (dialog->manager)
        g_object_unref (dialog->manager);
      dialog->manager = g_object_ref (g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
synced_tabs_dialog_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  SyncedTabsDialog *dialog = EPHY_SYNCED_TABS_DIALOG (object);

  switch (prop_id) {
    case PROP_OPEN_TABS_MANAGER:
      g_value_set_object (value, dialog->manager);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
synced_tabs_dialog_constructed (GObject *object)
{
  SyncedTabsDialog *dialog = EPHY_SYNCED_TABS_DIALOG (object);

  G_OBJECT_CLASS (synced_tabs_dialog_parent_class)->constructed (object);

  synced_tabs_dialog_populate_model (dialog);
}

static void
synced_tabs_dialog_dispose (GObject *object)
{
  SyncedTabsDialog *dialog = EPHY_SYNCED_TABS_DIALOG (object);

  g_clear_object (&dialog->manager);

  g_cancellable_cancel (dialog->cancellable);
  g_clear_object (&dialog->cancellable);

  G_OBJECT_CLASS (synced_tabs_dialog_parent_class)->dispose (object);
}

static void
synced_tabs_dialog_class_init (SyncedTabsDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = synced_tabs_dialog_set_property;
  object_class->get_property = synced_tabs_dialog_get_property;
  object_class->constructed = synced_tabs_dialog_constructed;
  object_class->dispose = synced_tabs_dialog_dispose;

  obj_properties[PROP_OPEN_TABS_MANAGER] =
    g_param_spec_object ("open-tabs-manager",
                         NULL, NULL,
                         EPHY_TYPE_OPEN_TABS_MANAGER,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/synced-tabs-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, SyncedTabsDialog, treestore);
  gtk_widget_class_bind_template_child (widget_class, SyncedTabsDialog, treeview);
  gtk_widget_class_bind_template_callback (widget_class, treeview_row_activated_cb);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static void
synced_tabs_dialog_init (SyncedTabsDialog *dialog)
{
  GtkTreeStore *store;

  gtk_widget_init_template (GTK_WIDGET (dialog));

  store = gtk_tree_store_new (3, G_TYPE_ICON, G_TYPE_STRING, G_TYPE_STRING);

  gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->treeview), GTK_TREE_MODEL (store));
  gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (dialog->treeview), URL_COLUMN);

  dialog->database = ephy_embed_shell_get_favicon_database (ephy_embed_shell_get_default ());
  dialog->cancellable = g_cancellable_new ();
}
G_GNUC_END_IGNORE_DEPRECATIONS

SyncedTabsDialog *
synced_tabs_dialog_new (EphyOpenTabsManager *manager)
{
  return EPHY_SYNCED_TABS_DIALOG (g_object_new (EPHY_TYPE_SYNCED_TABS_DIALOG,
                                                "open-tabs-manager", manager,
                                                NULL));
}
