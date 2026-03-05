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
  GtkWidget *list_box;
  GCancellable *cancellable;
};

G_DEFINE_FINAL_TYPE (SyncedTabsDialog, synced_tabs_dialog, ADW_TYPE_WINDOW)

enum {
  PROP_0,
  PROP_OPEN_TABS_MANAGER,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

typedef struct {
  GtkWidget *row;
  char *url;
} FaviconAsyncData;

static FaviconAsyncData *
favicon_async_data_new (GtkWidget  *row,
                        const char *url)
{
  FaviconAsyncData *data = g_new (FaviconAsyncData, 1);
  data->row = g_object_ref (row);
  data->url = g_strdup (url);
  return data;
}

static void
favicon_async_data_free (FaviconAsyncData *data)
{
  g_object_unref (data->row);
  g_free (data->url);
  g_free (data);
}

static void
tab_row_activated_cb (AdwActionRow *row,
                      gpointer      user_data)
{
  SyncedTabsDialog *dialog = EPHY_SYNCED_TABS_DIALOG (user_data);
  const char *url;
  EphyShell *shell;
  EphyEmbed *embed;
  GtkWindow *window;

  url = g_object_get_data (G_OBJECT (row), "tab-url");
  if (!url)
    return;

  shell = ephy_shell_get_default ();
  window = gtk_application_get_active_window (GTK_APPLICATION (shell));
  embed = ephy_shell_new_tab (shell, EPHY_WINDOW (window),
                              NULL, EPHY_NEW_TAB_JUMP);
  ephy_web_view_load_url (ephy_embed_get_web_view (embed), url);
  gtk_window_close (GTK_WINDOW (dialog));
}

static void
favicon_loaded_cb (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  WebKitFaviconDatabase *database = WEBKIT_FAVICON_DATABASE (source);
  FaviconAsyncData *data = user_data;
  g_autoptr (GdkTexture) texture = NULL;
  g_autoptr (GIcon) favicon = NULL;
  g_autoptr (GError) error = NULL;

  texture = webkit_favicon_database_get_favicon_finish (database, result, &error);
  if (!texture && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    favicon_async_data_free (data);
    return;
  }

  favicon = ephy_favicon_get_from_texture_scaled (texture, FAVICON_SIZE, FAVICON_SIZE);
  if (!favicon) {
    const char *icon_name = ephy_get_fallback_favicon_name (data->url, EPHY_FAVICON_TYPE_SHOW_MISSING_PLACEHOLDER);
    if (!icon_name)
      icon_name = "adw-tab-icon-missing-symbolic";
    favicon = g_themed_icon_new (icon_name);
  }

  adw_action_row_add_prefix (ADW_ACTION_ROW (data->row),
                             gtk_image_new_from_gicon (favicon));

  favicon_async_data_free (data);
}

static void
synced_tabs_dialog_populate_from_record (SyncedTabsDialog   *dialog,
                                         EphyOpenTabsRecord *record,
                                         gboolean            is_local)
{
  GtkWidget *expander;
  GList *tabs;
  const char *device_name;

  if (is_local)
    device_name = _("Local Tabs");
  else
    device_name = ephy_open_tabs_record_get_client_name (record);

  expander = adw_expander_row_new ();
  adw_expander_row_set_expanded (ADW_EXPANDER_ROW (expander), TRUE);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (expander), device_name);
  adw_expander_row_add_prefix (ADW_EXPANDER_ROW (expander),
                               gtk_image_new_from_icon_name ("computer-symbolic"));

  tabs = ephy_open_tabs_record_get_tabs (record);
  for (GList *l = tabs; l && l->data; l = l->next) {
    const char *title = json_object_get_string_member (l->data, "title");
    JsonArray *url_history = json_object_get_array_member (l->data, "urlHistory");
    const char *url = json_array_get_string_element (url_history, 0);
    GtkWidget *tab_row;

    tab_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (tab_row), title);
    adw_action_row_set_subtitle (ADW_ACTION_ROW (tab_row), url);
    gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (tab_row), !is_local);
    adw_action_row_set_subtitle_lines (ADW_ACTION_ROW (tab_row), 1);
    adw_action_row_set_title_lines (ADW_ACTION_ROW (tab_row), 1);
    g_object_set_data_full (G_OBJECT (tab_row), "tab-url", g_strdup (url), g_free);

    if (!is_local)
      g_signal_connect (tab_row, "activated",
                        G_CALLBACK (tab_row_activated_cb), dialog);

    /* Load favicon asynchronously. */
    webkit_favicon_database_get_favicon (dialog->database, url,
                                         dialog->cancellable,
                                         favicon_loaded_cb,
                                         favicon_async_data_new (tab_row, url));

    adw_expander_row_add_row (ADW_EXPANDER_ROW (expander), tab_row);
  }

  gtk_list_box_append (GTK_LIST_BOX (dialog->list_box), expander);
}

static void
synced_tabs_dialog_populate_model (SyncedTabsDialog *dialog)
{
  EphyOpenTabsRecord *record;
  GList *remotes;

  record = ephy_open_tabs_manager_get_local_tabs (dialog->manager);
  synced_tabs_dialog_populate_from_record (dialog, record, TRUE);

  remotes = ephy_open_tabs_manager_get_remote_tabs (dialog->manager);
  for (GList *l = remotes; l && l->data; l = l->next)
    synced_tabs_dialog_populate_from_record (dialog, l->data, FALSE);

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

  gtk_widget_class_bind_template_child (widget_class, SyncedTabsDialog, list_box);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);
}

static void
synced_tabs_dialog_init (SyncedTabsDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));

  dialog->database = ephy_embed_shell_get_favicon_database (ephy_embed_shell_get_default ());
  dialog->cancellable = g_cancellable_new ();
}

SyncedTabsDialog *
synced_tabs_dialog_new (EphyOpenTabsManager *manager)
{
  return EPHY_SYNCED_TABS_DIALOG (g_object_new (EPHY_TYPE_SYNCED_TABS_DIALOG,
                                                "open-tabs-manager", manager,
                                                NULL));
}
