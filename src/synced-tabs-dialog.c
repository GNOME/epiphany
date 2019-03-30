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

#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-favicon-helpers.h"
#include "ephy-shell.h"

#include <json-glib/json-glib.h>

#define PIXBUF_MISSING_PATH "/org/gnome/epiphany/web-watermark.svg"

struct _SyncedTabsDialog {
  GtkDialog parent_instance;

  EphyOpenTabsManager *manager;

  WebKitFaviconDatabase *database;
  GdkPixbuf *pixbuf_root;
  GdkPixbuf *pixbuf_missing;

  GtkTreeModel *treestore;
  GtkWidget *treeview;
};

G_DEFINE_TYPE (SyncedTabsDialog, synced_tabs_dialog, GTK_TYPE_DIALOG)

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
  char             *title;
  char             *url;
  guint             parent_index;
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
  cairo_surface_t *surface;
  GdkPixbuf *favicon = NULL;
  GtkTreeIter parent_iter;
  char *escaped_url;

  surface = webkit_favicon_database_get_favicon_finish (database, result, NULL);
  if (surface) {
    favicon = ephy_pixbuf_get_from_surface_scaled (surface, FAVICON_SIZE, FAVICON_SIZE);
    cairo_surface_destroy (surface);
  }

  gtk_tree_model_get_iter_first (data->dialog->treestore, &parent_iter);
  for (guint i = 0; i < data->parent_index; i++)
    gtk_tree_model_iter_next (data->dialog->treestore, &parent_iter);

  favicon = favicon ? favicon : data->dialog->pixbuf_missing;
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

  g_assert (EPHY_IS_SYNCED_TABS_DIALOG (dialog));
  g_assert (EPHY_IS_OPEN_TABS_RECORD (record));

  if (is_local)
    title = _("Local Tabs");
  else
    title = ephy_open_tabs_record_get_client_name (record);

  /* Insert top-level row. */
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (dialog->treestore),
                                     NULL, NULL, -1,
                                     ICON_COLUMN, dialog->pixbuf_root,
                                     TITLE_COLUMN, title,
                                     URL_COLUMN, NULL,
                                     -1);

  tabs = ephy_open_tabs_record_get_tabs (record);
  for (GList *l = tabs; l && l->data; l = l->next) {
    title = json_object_get_string_member (l->data, "title");
    url_history = json_object_get_array_member (l->data, "urlHistory");
    url = json_array_get_string_element (url_history, 0);

    data = populate_row_async_data_new (dialog, title, url, index);
    webkit_favicon_database_get_favicon (dialog->database, url, NULL,
                                         synced_tabs_dialog_favicon_loaded_cb,
                                         data);
  }
}

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
  g_clear_object (&dialog->pixbuf_root);
  g_clear_object (&dialog->pixbuf_missing);

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
                         "Open tabs manager",
                         "Open Tabs Manager",
                         EPHY_TYPE_OPEN_TABS_MANAGER,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/synced-tabs-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, SyncedTabsDialog, treestore);
  gtk_widget_class_bind_template_child (widget_class, SyncedTabsDialog, treeview);
  gtk_widget_class_bind_template_callback (widget_class, treeview_row_activated_cb);
}

static void
synced_tabs_dialog_init (SyncedTabsDialog *dialog)
{
  WebKitWebContext *context;
  GdkPixbuf *pixbuf;
  GError *error = NULL;

  gtk_widget_init_template (GTK_WIDGET (dialog));

  gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (dialog->treeview), URL_COLUMN);

  context = ephy_embed_shell_get_web_context (ephy_embed_shell_get_default ());
  dialog->database = webkit_web_context_get_favicon_database (context);

  dialog->pixbuf_root = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                                  "computer-symbolic",
                                                  FAVICON_SIZE, 0, &error);
  if (error) {
    g_warning ("Failed to build pixbuf from theme icon: %s", error->message);
    g_error_free (error);
    error = NULL;
  }

  pixbuf = gdk_pixbuf_new_from_resource (PIXBUF_MISSING_PATH, &error);
  if (pixbuf) {
    dialog->pixbuf_missing = gdk_pixbuf_scale_simple (pixbuf,
                                                      FAVICON_SIZE, FAVICON_SIZE,
                                                      GDK_INTERP_BILINEAR);
    g_object_unref (pixbuf);
  } else {
    g_warning ("Failed to build pixbuf from resource: %s", error->message);
    g_error_free (error);
  }
}

SyncedTabsDialog *
synced_tabs_dialog_new (EphyOpenTabsManager *manager)
{
  return EPHY_SYNCED_TABS_DIALOG (g_object_new (EPHY_TYPE_SYNCED_TABS_DIALOG,
                                                "use-header-bar", TRUE,
                                                "open-tabs-manager", manager,
                                                NULL));
}
