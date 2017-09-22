/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2009 Collabora Ltd.
 *  Copyright © 2016 Iulian-Gabriel Radu
 *  Copyright © 2011, 2017 Igalia S.L.
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

#include "window-commands.h"

#include "ephy-add-bookmark-popover.h"
#include "ephy-bookmarks-export.h"
#include "ephy-bookmarks-import.h"
#include "ephy-bookmarks-manager.h"
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-embed.h"
#include "ephy-encoding-dialog.h"
#include "ephy-favicon-helpers.h"
#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "ephy-find-toolbar.h"
#include "ephy-gui.h"
#include "ephy-header-bar.h"
#include "ephy-history-dialog.h"
#include "ephy-link.h"
#include "ephy-location-entry.h"
#include "ephy-notebook.h"
#include "ephy-prefs.h"
#include "ephy-session.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-web-app-utils.h"
#include "ephy-zoom.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libnotify/notify.h>
#include <libsoup/soup.h>
#include <string.h>
#include <webkit2/webkit2.h>

#define DEFAULT_ICON_SIZE 192
#define FAVICON_SIZE 16

void
window_cmd_new_window (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  EphyWindow *new_window;
  EphyShell *shell = ephy_shell_get_default ();

  if (ephy_embed_shell_get_mode (EPHY_EMBED_SHELL (shell)) == EPHY_EMBED_SHELL_MODE_INCOGNITO) {
    ephy_open_default_instance_window ();
    return;
  }

  new_window = ephy_window_new ();
  ephy_link_open (EPHY_LINK (new_window), NULL, NULL, EPHY_LINK_HOME_PAGE);
}

void
window_cmd_new_incognito_window (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       user_data)
{
  ephy_open_incognito_window (NULL);
}

const gchar *import_option_names[2] = {
  N_("GVDB File"),
  N_("Firefox")
};

static void
combo_box_changed_cb (GtkComboBox *combo_box,
                      GtkButton   *button)
{
  int active;

  g_assert (GTK_IS_COMBO_BOX (combo_box));
  g_assert (GTK_IS_BUTTON (button));

  active = gtk_combo_box_get_active (combo_box);
  if (active == 0)
    gtk_button_set_label (button, _("Ch_oose File"));
  else if (active == 1)
    gtk_button_set_label (button, _("I_mport"));
}

static gchar *
get_path (GIOChannel *channel)
{
  gchar *line;
  gchar *path;
  gsize length;

  do {
    g_io_channel_read_line (channel, &line, &length, NULL, NULL);

    if (g_str_has_prefix (line, "Path")) {
      path = g_strdup (line);

      /* Extract value (e.g. Path=Value\n -> Value) */
      path = strchr (path, '=');
      path++;
      path[strcspn (path, "\n")] = 0;

      g_free (line);
      return path;
    }

    g_free (line);
    /* Until '\n' */
  } while (length != 1);

  return NULL;
}

static GSList *
get_firefox_profiles (void)
{
  GIOChannel *channel;
  GSList *profiles = NULL;
  gchar *filename;
  gchar *line;
  gchar *profile;
  int count = 0;
  gsize length;

  filename = g_build_filename (g_get_home_dir (),
                               FIREFOX_PROFILES_DIR,
                               FIREFOX_PROFILES_FILE,
                               NULL);
  channel = g_io_channel_new_file (filename, "r", NULL);
  g_free (filename);

  if (channel) {
    do {
      g_io_channel_read_line (channel, &line, &length, NULL, NULL);

      profile = g_strdup_printf ("[Profile%d]\n", count);
      if (g_strcmp0 (line, profile) == 0) {
        profiles = g_slist_append (profiles, get_path (channel));

        count++;
      }
      g_free (profile);
      g_free (line);
    } while (length != 0);
  }

  return profiles;
}

static GtkTreeModel *
create_tree_model (void)
{
  enum {
    TEXT_COL
  };
  GtkListStore *list_store;
  GtkTreeIter iter;
  GSList *firefox_profiles;
  gboolean has_firefox_profile;
  int i;


  /* Check if user has a firefox profile*/
  firefox_profiles = get_firefox_profiles ();
  has_firefox_profile = g_slist_length (firefox_profiles) > 0;
  g_slist_free (firefox_profiles);

  list_store = gtk_list_store_new (1, G_TYPE_STRING);
  for (i = G_N_ELEMENTS (import_option_names) - 1; i >= 0; i--) {
    /* Skip Firefox option if user doesn't have a Firefox profile */
    if (g_strcmp0 (import_option_names[i], _("Firefox")) == 0) {
      if (!has_firefox_profile)
        continue;
    }

    gtk_list_store_prepend (list_store, &iter);
    gtk_list_store_set (list_store, &iter,
                        TEXT_COL, _(import_option_names[i]),
                        -1);
  }

  return GTK_TREE_MODEL (list_store);
}

static gchar *
show_profile_selector (GtkWidget *parent, GSList *profiles)
{
  GtkWidget *selector;
  GtkWidget *list_box;
  GtkWidget *suggested;
  GtkWidget *content_area;
  GSList *l;
  int response;
  gchar *selected_profile = NULL;

  selector = gtk_dialog_new_with_buttons (_("Select Profile"),
                                          GTK_WINDOW (parent),
                                          GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
                                          _("_Cancel"),
                                          GTK_RESPONSE_CANCEL,
                                          _("_Select"),
                                          GTK_RESPONSE_OK,
                                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (selector), GTK_RESPONSE_OK);

  suggested = gtk_dialog_get_widget_for_response (GTK_DIALOG (selector), GTK_RESPONSE_OK);
  gtk_style_context_add_class (gtk_widget_get_style_context (suggested),
                               GTK_STYLE_CLASS_SUGGESTED_ACTION);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (selector));
  gtk_container_set_border_width (GTK_CONTAINER (content_area), 5);
  gtk_widget_set_valign (content_area, GTK_ALIGN_CENTER);

  list_box = gtk_list_box_new ();
  for (l = profiles; l != NULL; l = l->next) {
    const gchar *profile = l->data;
    GtkWidget *label;

    label = gtk_label_new (strchr (profile, '.') + 1);
    g_object_set_data (G_OBJECT (label), "profile_path", g_strdup (profile));
    gtk_widget_set_margin_top (label, 6);
    gtk_widget_set_margin_bottom (label, 6);
    gtk_list_box_insert (GTK_LIST_BOX (list_box), label, -1);
  }
  gtk_container_add (GTK_CONTAINER (content_area), list_box);

  gtk_widget_show_all (content_area);

  response = gtk_dialog_run (GTK_DIALOG (selector));
  if (response == GTK_RESPONSE_OK) {
    GtkListBoxRow *row;
    GtkWidget *row_widget;

    row = gtk_list_box_get_selected_row (GTK_LIST_BOX (list_box));
    row_widget = gtk_bin_get_child (GTK_BIN (row));
    selected_profile = g_object_get_data (G_OBJECT (row_widget), "profile_path");
  }
  gtk_widget_destroy (selector);

  return selected_profile;
}

static void
dialog_bookmarks_import_cb (GtkDialog   *dialog,
                            int          response,
                            GtkComboBox *combo_box)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  GtkFileChooser *file_chooser_dialog;
  GtkWidget *import_info_dialog;
  int active;
  int chooser_response;
  gboolean imported = FALSE;

  if (response == GTK_RESPONSE_OK) {
    active = gtk_combo_box_get_active (combo_box);
    if (active == 0) {
      GtkFileFilter *filter;

      file_chooser_dialog = GTK_FILE_CHOOSER (gtk_file_chooser_native_new (_("Choose File"),
                                                                           GTK_WINDOW (dialog),
                                                                           GTK_FILE_CHOOSER_ACTION_OPEN,
                                                                           _("I_mport"),
                                                                           _("_Cancel")));
      gtk_file_chooser_set_show_hidden (file_chooser_dialog, TRUE);

      filter = gtk_file_filter_new ();
      gtk_file_filter_add_pattern (filter, "*.gvdb");
      gtk_file_chooser_set_filter (file_chooser_dialog, filter);

      chooser_response = gtk_native_dialog_run (GTK_NATIVE_DIALOG (file_chooser_dialog));
      if (chooser_response == GTK_RESPONSE_ACCEPT) {
        GError *error = NULL;
        char *filename;

        gtk_native_dialog_hide (GTK_NATIVE_DIALOG (file_chooser_dialog));

        filename = gtk_file_chooser_get_filename (file_chooser_dialog);
        imported = ephy_bookmarks_import (manager, filename, &error);
        g_free (filename);

        import_info_dialog = gtk_message_dialog_new (GTK_WINDOW (dialog),
                                                     GTK_DIALOG_MODAL,
                                                     imported ? GTK_MESSAGE_INFO : GTK_MESSAGE_WARNING,
                                                     GTK_BUTTONS_OK,
                                                     "%s",
                                                     imported ? _("Bookmarks successfully imported!") :
                                                                error->message);
        gtk_dialog_run (GTK_DIALOG (import_info_dialog));

        gtk_widget_destroy (import_info_dialog);
      }
      g_object_unref (file_chooser_dialog);
    } else if (active == 1) {
      GError *error = NULL;
      GSList *profiles;
      gchar *profile = NULL;
      int num_profiles;

      profiles = get_firefox_profiles ();

      /* Import default profile */
      num_profiles = g_slist_length (profiles);
      if (num_profiles == 1) {
        imported = ephy_bookmarks_import_from_firefox (manager, profiles->data, &error);
      } else if (num_profiles > 1) {
        profile = show_profile_selector (GTK_WIDGET (dialog), profiles);
        if (profile) {
          imported = ephy_bookmarks_import_from_firefox (manager, profile, &error);
          g_free (profile);
        }
      } else {
        g_assert_not_reached ();
      }

      g_slist_free (profiles);

      /* If there are multiple profiles, but the user didn't select one in
       * the profile (he pressed Cancel), don't display the import info dialog
       * as no import took place
       */
      if (profile) {
        import_info_dialog = gtk_message_dialog_new (GTK_WINDOW (dialog),
                                                     GTK_DIALOG_MODAL,
                                                     imported ? GTK_MESSAGE_INFO : GTK_MESSAGE_WARNING,
                                                     GTK_BUTTONS_OK,
                                                     "%s",
                                                     imported ? _("Bookmarks successfully imported!") :
                                                                error->message);
        gtk_dialog_run (GTK_DIALOG (import_info_dialog));
        gtk_widget_destroy (import_info_dialog);
      }
      if (error)
        g_error_free (error);
    }

    if (imported)
      gtk_widget_destroy (GTK_WIDGET (dialog));
  } else if (response == GTK_RESPONSE_CANCEL) {
    gtk_widget_destroy (GTK_WIDGET (dialog));
  }
}

void
window_cmd_import_bookmarks (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  GtkWidget *dialog;
  GtkWidget *content_area;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *combo_box;
  GtkTreeModel *tree_model;
  GtkCellRenderer *cell_renderer;

  dialog = gtk_dialog_new_with_buttons (_("Import Bookmarks"),
                                        GTK_WINDOW (window),
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
                                        _("_Cancel"),
                                        GTK_RESPONSE_CANCEL,
                                        _("Ch_oose File"),
                                        GTK_RESPONSE_OK,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_widget_set_valign (content_area, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start (content_area, 25);
  gtk_widget_set_margin_end (content_area, 25);
  gtk_container_set_border_width (GTK_CONTAINER (content_area), 5);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);

  label = gtk_label_new (_("From:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  tree_model = create_tree_model ();
  combo_box = gtk_combo_box_new_with_model (GTK_TREE_MODEL (tree_model));
  g_object_unref (tree_model);
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);

  g_signal_connect (GTK_COMBO_BOX (combo_box), "changed",
                    G_CALLBACK (combo_box_changed_cb),
                    gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK));

  cell_renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), cell_renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), cell_renderer,
                                  "text", 0, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), combo_box, TRUE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (content_area), hbox);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  g_signal_connect (dialog, "response",
                    G_CALLBACK (dialog_bookmarks_import_cb),
                    GTK_COMBO_BOX (combo_box));

  gtk_widget_show_all (dialog);
}

void
window_cmd_export_bookmarks (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  GtkFileChooser *dialog;
  GtkWidget *export_info_dialog;
  int chooser_response;
  gboolean exported;
  GtkFileFilter *filter;

  dialog = GTK_FILE_CHOOSER (gtk_file_chooser_native_new (_("Choose File"),
                                                          GTK_WINDOW (user_data),
                                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                                          _("_Save"),
                                                          _("_Cancel")));
  gtk_file_chooser_set_show_hidden (dialog, TRUE);

  /* Translators: Only translate the part before ".gvdb" (e.g. "bookmarks") */
  gtk_file_chooser_set_current_name (dialog, _("bookmarks.gvdb"));

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*.gvdb");
  gtk_file_chooser_set_filter (dialog, filter);

  chooser_response = gtk_native_dialog_run (GTK_NATIVE_DIALOG (dialog));
  if (chooser_response == GTK_RESPONSE_ACCEPT) {
    GError *error = NULL;
    char *filename;

    gtk_native_dialog_hide (GTK_NATIVE_DIALOG (dialog));

    filename = gtk_file_chooser_get_filename (dialog);
    exported = ephy_bookmarks_export (manager, filename, &error);
    g_free (filename);

    export_info_dialog = gtk_message_dialog_new (GTK_WINDOW (user_data),
                                                 GTK_DIALOG_MODAL,
                                                 exported ? GTK_MESSAGE_INFO : GTK_MESSAGE_WARNING,
                                                 GTK_BUTTONS_OK,
                                                 "%s",
                                                 exported ? _("Bookmarks successfully exported!") :
                                                            error->message);
    gtk_dialog_run (GTK_DIALOG (export_info_dialog));
    gtk_widget_destroy (export_info_dialog);
  }

  g_object_unref (dialog);
}

void
window_cmd_show_history (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       user_data)
{
  GtkWidget *dialog;

  dialog = ephy_shell_get_history_dialog (ephy_shell_get_default ());

  if (GTK_WINDOW (user_data) != gtk_window_get_transient_for (GTK_WINDOW (dialog)))
    gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                  GTK_WINDOW (user_data));
  gtk_window_present (GTK_WINDOW (dialog));
}

void
window_cmd_show_preferences (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  GtkWindow *dialog;

  dialog = GTK_WINDOW (ephy_shell_get_prefs_dialog (ephy_shell_get_default ()));

  if (GTK_WINDOW (user_data) != gtk_window_get_transient_for (dialog))
    gtk_window_set_transient_for (dialog,
                                  GTK_WINDOW (user_data));

  gtk_window_present (dialog);
}

void
window_cmd_show_shortcuts (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  static GtkWidget *shortcuts_window;

  if (shortcuts_window == NULL) {
    GtkBuilder *builder;

    builder = gtk_builder_new_from_resource ("/org/gnome/epiphany/gtk/shortcuts-dialog.ui");
    shortcuts_window = GTK_WIDGET (gtk_builder_get_object (builder, "shortcuts-dialog"));

    g_signal_connect (shortcuts_window,
                      "destroy",
                      G_CALLBACK (gtk_widget_destroyed),
                      &shortcuts_window);

    g_object_unref (builder);
  }

  if (gtk_window_get_transient_for (GTK_WINDOW (shortcuts_window)) != GTK_WINDOW (user_data))
    gtk_window_set_transient_for (GTK_WINDOW (shortcuts_window), GTK_WINDOW (user_data));

  gtk_window_present (GTK_WINDOW (shortcuts_window));
}

void
window_cmd_show_help (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  ephy_gui_help (GTK_WIDGET (user_data), NULL);
}

#define ABOUT_GROUP "About"

void
window_cmd_show_about (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  char *comments = NULL;
  GKeyFile *key_file;
  GBytes *bytes;
  GError *error = NULL;
  char **list, **authors, **contributors, **past_authors, **artists, **documenters;
  gsize n_authors, n_contributors, n_past_authors, n_artists, n_documenters, i, j;

  key_file = g_key_file_new ();
  bytes = g_resources_lookup_data ("/org/gnome/epiphany/about.ini", 0, NULL);
  if (!g_key_file_load_from_data (key_file, g_bytes_get_data (bytes, NULL), -1, 0, &error)) {
    g_warning ("Couldn't load about data: %s\n", error->message);
    g_error_free (error);
    return;
  }
  g_bytes_unref (bytes);

  list = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Authors",
                                     &n_authors, NULL);
  contributors = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Contributors",
                                             &n_contributors, NULL);
  past_authors = g_key_file_get_string_list (key_file, ABOUT_GROUP, "PastAuthors",
                                             &n_past_authors, NULL);

#define APPEND(_to, _from) \
  _to[i++] = g_strdup (_from);

#define APPEND_STRV_AND_FREE(_to, _from) \
  if (_from) \
  { \
    for (j = 0; _from[j] != NULL; ++j) \
    { \
      _to[i++] = _from[j]; \
    } \
    g_free (_from); \
  }

  authors = g_new (char *, (list ? n_authors : 0) +
                   (contributors ? n_contributors : 0) +
                   (past_authors ? n_past_authors : 0) + 7 + 1);
  i = 0;
  APPEND_STRV_AND_FREE (authors, list);
  APPEND (authors, "");
  APPEND (authors, _("Contact us at:"));
  APPEND (authors, "<epiphany-list@gnome.org>");
  APPEND (authors, "");
  APPEND (authors, _("Contributors:"));
  APPEND_STRV_AND_FREE (authors, contributors);
  APPEND (authors, "");
  APPEND (authors, _("Past developers:"));
  APPEND_STRV_AND_FREE (authors, past_authors);
  authors[i++] = NULL;

  list = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Artists", &n_artists, NULL);

  artists = g_new (char *, (list ? n_artists : 0) + 4 + 1);
  i = 0;
  APPEND_STRV_AND_FREE (artists, list);
  artists[i++] = NULL;

  list = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Documenters", &n_documenters, NULL);

  documenters = g_new (char *, (list ? n_documenters : 0) + 3 + 1);
  i = 0;
  APPEND_STRV_AND_FREE (documenters, list);
  APPEND (documenters, "");
  APPEND (documenters, _("Contact us at:"));
  APPEND (documenters, "<gnome-doc-list@gnome.org>");
  documenters[i++] = NULL;

#undef APPEND
#undef APPEND_STRV_AND_FREE

  g_key_file_free (key_file);

  comments = g_strdup_printf (_("A simple, clean, beautiful view of the web.\n"
                                "Powered by WebKitGTK+ %d.%d.%d"),
                              webkit_get_major_version (),
                              webkit_get_minor_version (),
                              webkit_get_micro_version ());

  gtk_show_about_dialog (window ? GTK_WINDOW (window) : NULL,
                         "program-name", _("Web"),
                         "version", VERSION,
                         "copyright", "Copyright © 2002–2004 Marco Pesenti Gritti\n"
                         "Copyright © 2003–2017 The Web Developers",
                         "artists", artists,
                         "authors", authors,
                         "comments", comments,
                         "documenters", documenters,
                         /* Translators: This is a special message that shouldn't be translated
                          * literally. It is used in the about box to give credits to
                          * the translators.
                          * Thus, you should translate it to your name and email address.
                          * You should also include other translators who have contributed to
                          * this translation; in that case, please write each of them on a separate
                          * line seperated by newlines (\n).
                          */
                         "translator-credits", _("translator-credits"),
                         "logo-icon-name", "org.gnome.Epiphany",
                         "website", "https://wiki.gnome.org/Apps/Web",
                         "website-label", _("Web Website"),
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "wrap-license", TRUE,
                         NULL);

  g_free (comments);
  g_strfreev (artists);
  g_strfreev (authors);
  g_strfreev (documenters);
}

void
window_cmd_quit (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  ephy_shell_try_quit (ephy_shell_get_default ());
}

void
window_cmd_reopen_closed_tab (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  EphySession *session = ephy_shell_get_session (ephy_shell_get_default ());

  g_assert (session != NULL);
  ephy_session_undo_close_tab (session);
}

void
window_cmd_navigation (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyEmbed *embed;
  WebKitWebView *web_view;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_return_if_fail (embed != NULL);

  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  if (strstr (g_action_get_name (G_ACTION (action)), "back")) {
    webkit_web_view_go_back (web_view);
    gtk_widget_grab_focus (GTK_WIDGET (embed));
  } else {
    webkit_web_view_go_forward (web_view);
    gtk_widget_grab_focus (GTK_WIDGET (embed));
  }
}

void
window_cmd_navigation_new_tab (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyEmbed *embed;
  WebKitWebView *web_view;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_return_if_fail (embed != NULL);

  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  if (strstr (g_action_get_name (G_ACTION (action)), "back")) {
      const char *back_uri;
      WebKitBackForwardList *history;
      WebKitBackForwardListItem *back_item;

      history = webkit_web_view_get_back_forward_list (web_view);
      back_item = webkit_back_forward_list_get_back_item (history);
      back_uri = webkit_back_forward_list_item_get_original_uri (back_item);

      embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
                                  NULL,
                                  0);

      web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
      webkit_web_view_load_uri (web_view, back_uri);
      gtk_widget_grab_focus (GTK_WIDGET (embed));
  } else {
      const char *forward_uri;
      WebKitBackForwardList *history;
      WebKitBackForwardListItem *forward_item;

      /* Forward history is not copied when opening
         a new tab, so get the forward URI manually
         and load it */
      history = webkit_web_view_get_back_forward_list (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));
      forward_item = webkit_back_forward_list_get_forward_item (history);
      forward_uri = webkit_back_forward_list_item_get_original_uri (forward_item);

      embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
                                  embed,
                                  0);

      web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
      webkit_web_view_load_uri (web_view, forward_uri);
  }
}

void
window_cmd_stop (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyEmbed *embed;

  embed = ephy_embed_container_get_active_child
            (EPHY_EMBED_CONTAINER (window));
  g_return_if_fail (embed != NULL);

  gtk_widget_grab_focus (GTK_WIDGET (embed));

  webkit_web_view_stop_loading (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));
}

static gboolean
event_with_shift (void)
{
  GdkEvent *event;
  GdkEventType type = 0;
  guint state = 0;

  event = gtk_get_current_event ();
  if (event) {
    type = event->type;

    if (type == GDK_BUTTON_RELEASE) {
      state = event->button.state;
    } else if (type == GDK_KEY_PRESS || type == GDK_KEY_RELEASE) {
      state = event->key.state;
    }

    gdk_event_free (event);
  }

  return (state & GDK_SHIFT_MASK) != 0;
}

void
window_cmd_reload (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyEmbed *embed;
  WebKitWebView *view;

  embed = ephy_embed_container_get_active_child
            (EPHY_EMBED_CONTAINER (window));
  g_return_if_fail (embed != NULL);

  gtk_widget_grab_focus (GTK_WIDGET (embed));

  view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
  if (event_with_shift ())
    webkit_web_view_reload_bypass_cache (view);
  else
    webkit_web_view_reload (view);
}

void window_cmd_combined_stop_reload (GSimpleAction *action,
                                      GVariant      *parameter,
                                      gpointer       user_data)
{
  GActionGroup *action_group;
  GAction *gaction;
  GVariant *state;

  action_group = gtk_widget_get_action_group (GTK_WIDGET (user_data), "toolbar");

  state = g_action_get_state (G_ACTION (action));
  /* If loading */
  if (g_variant_get_boolean (state))
    gaction = g_action_map_lookup_action (G_ACTION_MAP (action_group), "stop");
  else
    gaction = g_action_map_lookup_action (G_ACTION_MAP (action_group), "reload");

  g_action_activate (gaction, NULL);

  g_variant_unref (state);
}

void
window_cmd_new_tab (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  EphyWindow *window = user_data;

  ephy_link_open (EPHY_LINK (window),
                  NULL, NULL,
                  EPHY_LINK_NEW_TAB | EPHY_LINK_JUMP_TO);
}

static void
open_response_cb (GtkNativeDialog *dialog, int response, EphyWindow *window)
{
  if (response == GTK_RESPONSE_ACCEPT) {
    char *uri, *converted;

    uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
    if (uri != NULL) {
      converted = g_filename_to_utf8 (uri, -1, NULL, NULL, NULL);

      if (converted != NULL) {
        ephy_window_load_url (window, converted);
      }

      g_free (converted);
      g_free (uri);
    }
  }

  g_object_unref (dialog);
}

void
window_cmd_open (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  EphyWindow *window = user_data;
  GtkFileChooser *dialog;

  dialog = ephy_create_file_chooser (_("Open"),
                                     GTK_WIDGET (window),
                                     GTK_FILE_CHOOSER_ACTION_OPEN,
                                     EPHY_FILE_FILTER_ALL_SUPPORTED);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (open_response_cb), window);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));
}

typedef struct {
  EphyWebView *view;
  GtkWidget *image;
  GtkWidget *entry;
  GtkWidget *spinner;
  GtkWidget *box;
  char *icon_href;
  GdkRGBA icon_rgba;
} EphyApplicationDialogData;

static void
ephy_application_dialog_data_free (EphyApplicationDialogData *data)
{
  g_free (data->icon_href);
  g_slice_free (EphyApplicationDialogData, data);
}

static void
rounded_rectangle (cairo_t *cr,
                   gdouble  aspect,
                   gdouble  x,
                   gdouble  y,
                   gdouble  corner_radius,
                   gdouble  width,
                   gdouble  height)
{
  gdouble radius;
  gdouble degrees;

  radius = corner_radius / aspect;
  degrees = G_PI / 180.0;

  cairo_new_sub_path (cr);
  cairo_arc (cr,
             x + width - radius,
             y + radius,
             radius,
             -90 * degrees,
             0 * degrees);
  cairo_arc (cr,
             x + width - radius,
             y + height - radius,
             radius,
             0 * degrees,
             90 * degrees);
  cairo_arc (cr,
             x + radius,
             y + height - radius,
             radius,
             90 * degrees,
             180 * degrees);
  cairo_arc (cr,
             x + radius,
             y + radius,
             radius,
             180 * degrees,
             270 * degrees);
  cairo_close_path (cr);
}

static GdkPixbuf *
frame_pixbuf (GdkPixbuf *pixbuf,
              GdkRGBA   *rgba,
              int        width,
              int        height)
{
  GdkPixbuf *framed;
  cairo_surface_t *surface;
  cairo_t *cr;
  int frame_width;
  int radius;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        width, height);
  cr = cairo_create (surface);

  frame_width = 0;
  radius = 20;

  rounded_rectangle (cr,
                     1.0,
                     frame_width + 0.5,
                     frame_width + 0.5,
                     radius,
                     width - frame_width * 2 - 1,
                     height - frame_width * 2 - 1);
  if (rgba != NULL)
    cairo_set_source_rgba (cr,
                           rgba->red,
                           rgba->green,
                           rgba->blue,
                           rgba->alpha);
  else
    cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.3);
  cairo_fill_preserve (cr);

  if (pixbuf != NULL) {
    GdkPixbuf *scaled;
    int w;
    int h;

    w = gdk_pixbuf_get_width (pixbuf);
    h = gdk_pixbuf_get_height (pixbuf);

    if (w < 48 || h < 48) {
      scaled = gdk_pixbuf_scale_simple (pixbuf, w * 3, h * 3, GDK_INTERP_NEAREST);
    } else if (w > width || h > height) {
      double ws, hs, s;

      ws = (double)width / w;
      hs = (double)height / h;
      s = MIN (ws, hs);
      scaled = gdk_pixbuf_scale_simple (pixbuf, w * s, h * s, GDK_INTERP_BILINEAR);
    } else {
      scaled = g_object_ref (pixbuf);
    }

    w = gdk_pixbuf_get_width (scaled);
    h = gdk_pixbuf_get_height (scaled);

    gdk_cairo_set_source_pixbuf (cr, scaled,
                                 (width - w) / 2,
                                 (height - h) / 2);
    g_object_unref (scaled);
    cairo_fill (cr);
  }

  framed = gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  return framed;
}

static void
set_image_from_favicon (EphyApplicationDialogData *data)
{
  GdkPixbuf *icon = NULL;
  cairo_surface_t *icon_surface = webkit_web_view_get_favicon (WEBKIT_WEB_VIEW (data->view));

  if (icon_surface)
    icon = ephy_pixbuf_get_from_surface_scaled (icon_surface, 0, 0);

  if (icon != NULL) {
    GdkPixbuf *framed;

    framed = frame_pixbuf (icon, NULL, DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE);
    g_object_unref (icon);
    gtk_image_set_from_pixbuf (GTK_IMAGE (data->image), framed);
    g_object_unref (framed);
  }
}

static void
set_app_icon_from_filename (EphyApplicationDialogData *data,
                            const char                *filename)
{
  GdkPixbuf *pixbuf;
  GdkPixbuf *framed;

  pixbuf = gdk_pixbuf_new_from_file_at_size (filename, DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE, NULL);
  if (pixbuf == NULL)
    return;

  framed = frame_pixbuf (pixbuf, &data->icon_rgba, DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE);
  g_object_unref (pixbuf);
  gtk_image_set_from_pixbuf (GTK_IMAGE (data->image), framed);
  g_object_unref (framed);
}

static void
download_finished_cb (WebKitDownload            *download,
                      EphyApplicationDialogData *data)
{
  char *filename;

  gtk_widget_show (data->image);

  filename = g_filename_from_uri (webkit_download_get_destination (download), NULL, NULL);
  set_app_icon_from_filename (data, filename);
  g_free (filename);
}

static void
download_failed_cb (WebKitDownload            *download,
                    GError                    *error,
                    EphyApplicationDialogData *data)
{
  gtk_widget_show (data->image);

  g_signal_handlers_disconnect_by_func (download, download_finished_cb, data);
  /* Something happened, default to a page snapshot. */
  set_image_from_favicon (data);
}

static void
download_icon_and_set_image (EphyApplicationDialogData *data)
{
  WebKitDownload *download;
  char *destination, *destination_uri, *tmp_filename;
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();

  download = webkit_web_context_download_uri (ephy_embed_shell_get_web_context (shell),
                                              data->icon_href);
  /* We do not want this download to show up in the UI, so let's
   * set 'ephy-download-set' to make Epiphany think this is
   * already there. */
  /* FIXME: it's probably better to just do this in a clean way
   * instead of using this workaround. */
  g_object_set_data (G_OBJECT (download), "ephy-download-set", GINT_TO_POINTER (TRUE));

  tmp_filename = ephy_file_tmp_filename (".ephy-download-XXXXXX", NULL);
  destination = g_build_filename (ephy_file_tmp_dir (), tmp_filename, NULL);
  destination_uri = g_filename_to_uri (destination, NULL, NULL);
  webkit_download_set_destination (download, destination_uri);
  g_free (destination);
  g_free (destination_uri);
  g_free (tmp_filename);

  g_signal_connect (download, "finished",
                    G_CALLBACK (download_finished_cb), data);
  g_signal_connect (download, "failed",
                    G_CALLBACK (download_failed_cb), data);
}

static void
fill_default_application_image_cb (GObject      *source,
                                   GAsyncResult *async_result,
                                   gpointer      user_data)
{
  EphyApplicationDialogData *data = user_data;
  char *uri = NULL;
  GdkRGBA color = { 0.5, 0.5, 0.5, 0.3 };

  ephy_web_view_get_best_web_app_icon_finish (EPHY_WEB_VIEW (source), async_result, &uri, &color, NULL);

  data->icon_href = uri;
  data->icon_rgba = color;
  if (data->icon_href != NULL)
    download_icon_and_set_image (data);
  else {
    gtk_widget_show (data->image);
    set_image_from_favicon (data);
  }
}

static void
fill_default_application_image (EphyApplicationDialogData *data)
{
  ephy_web_view_get_best_web_app_icon (data->view, NULL, fill_default_application_image_cb, data);
}

typedef struct {
  const char *host;
  const char *name;
} SiteInfo;

static SiteInfo sites[] = {
  { "www.facebook.com", "Facebook" },
  { "twitter.com", "Twitter" },
  { "gmail.com", "GMail" },
  { "plus.google.com", "Google+" },
  { "youtube.com", "YouTube" },
};

static char *
get_special_case_application_title_for_host (const char *host)
{
  char *title = NULL;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (sites) && title == NULL; i++) {
    SiteInfo *info = &sites[i];
    if (strcmp (host, info->host) == 0) {
      title = g_strdup (info->name);
    }
  }

  return title;
}

static void
set_default_application_title (EphyApplicationDialogData *data,
                               char                      *title)
{
  if (title == NULL || title[0] == '\0') {
    SoupURI *uri;
    const char *host;

    uri = soup_uri_new (webkit_web_view_get_uri (WEBKIT_WEB_VIEW (data->view)));
    host = soup_uri_get_host (uri);

    if (host != NULL && host[0] != '\0')
      title = get_special_case_application_title_for_host (host);

    if (title == NULL || title[0] == '\0') {
      if (g_str_has_prefix (host, "www."))
        title = g_strdup (host + strlen ("www."));
      else
        title = g_strdup (host);
    }

    soup_uri_free (uri);
  }

  if (title == NULL || title[0] == '\0') {
    title = g_strdup (webkit_web_view_get_title (WEBKIT_WEB_VIEW (data->view)));
  }

  gtk_entry_set_text (GTK_ENTRY (data->entry), title);
  g_free (title);
}

static void
fill_default_application_title_cb (GObject      *source,
                                   GAsyncResult *async_result,
                                   gpointer      user_data)
{
  EphyApplicationDialogData *data = user_data;
  char *title;

  title = ephy_web_view_get_web_app_title_finish (EPHY_WEB_VIEW (source), async_result, NULL);
  set_default_application_title (data, title);
}

static void
fill_default_application_title (EphyApplicationDialogData *data)
{
  ephy_web_view_get_web_app_title (data->view, NULL, fill_default_application_title_cb, data);
}

static void
notify_launch_cb (NotifyNotification *notification,
                  char               *action,
                  gpointer            user_data)
{
  char *desktop_file = user_data;

  ephy_file_launch_desktop_file (desktop_file, NULL, 0, NULL);
  g_free (desktop_file);
}

static gboolean
confirm_web_application_overwrite (GtkWindow *parent, const char *title)
{
  GtkResponseType response;
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (parent,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("A web application named “%s” already exists. Do you want to replace it?"),
                                   title);
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("Cancel"),
                          GTK_RESPONSE_CANCEL,
                          _("Replace"),
                          GTK_RESPONSE_OK,
                          NULL);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("An application with the same name already exists. Replacing it will "
                                              "overwrite it."));
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);

  return response == GTK_RESPONSE_OK;
}

static void
dialog_save_as_application_response_cb (GtkDialog                 *dialog,
                                        gint                       response,
                                        EphyApplicationDialogData *data)
{
  const char *app_name;
  char *desktop_file;
  char *message;
  NotifyNotification *notification;

  if (response == GTK_RESPONSE_OK) {
    app_name = gtk_entry_get_text (GTK_ENTRY (data->entry));

    if (ephy_web_application_exists (app_name)) {
      if (confirm_web_application_overwrite (GTK_WINDOW (dialog), app_name))
        ephy_web_application_delete (app_name);
      else
        return;
    }

    /* Create Web Application, including a new profile and .desktop file. */
    desktop_file = ephy_web_application_create (webkit_web_view_get_uri (WEBKIT_WEB_VIEW (data->view)),
                                                app_name,
                                                gtk_image_get_pixbuf (GTK_IMAGE (data->image)));
    if (desktop_file)
      message = g_strdup_printf (_("The application “%s” is ready to be used"),
                                 app_name);
    else
      message = g_strdup_printf (_("The application “%s” could not be created"),
                                 app_name);

    notification = notify_notification_new (message,
                                            NULL, NULL);
    g_free (message);

    if (desktop_file) {
      notify_notification_add_action (notification, "launch", _("Launch"),
                                      (NotifyActionCallback)notify_launch_cb,
                                      g_path_get_basename (desktop_file),
                                      NULL);
      notify_notification_set_icon_from_pixbuf (notification, gtk_image_get_pixbuf (GTK_IMAGE (data->image)));
      g_free (desktop_file);
    }

    notify_notification_set_timeout (notification, NOTIFY_EXPIRES_DEFAULT);
    notify_notification_set_urgency (notification, NOTIFY_URGENCY_LOW);
    notify_notification_set_hint (notification, "desktop-entry", g_variant_new_string ("epiphany"));
    notify_notification_set_hint (notification, "transient", g_variant_new_boolean (TRUE));
    notify_notification_show (notification, NULL);
  }

  ephy_application_dialog_data_free (data);
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

void
window_cmd_save_as_application (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyEmbed *embed;
  GtkWidget *dialog, *box, *image, *entry, *content_area;
  GtkWidget *label;
  GtkWidget *spinner;
  EphyWebView *view;
  EphyApplicationDialogData *data;
  GdkPixbuf *pixbuf;
  GtkStyleContext *context;
  char *markup;
  char *escaped_address;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_return_if_fail (embed != NULL);

  view = EPHY_WEB_VIEW (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));

  /* Show dialog with icon, title. */
  dialog = gtk_dialog_new_with_buttons (_("Create Web Application"),
                                        GTK_WINDOW (window),
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
                                        _("_Cancel"),
                                        GTK_RESPONSE_CANCEL,
                                        _("C_reate"),
                                        GTK_RESPONSE_OK,
                                        NULL);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add (GTK_CONTAINER (content_area), box);
  gtk_container_set_border_width (GTK_CONTAINER (box), 5);

  image = gtk_image_new ();
  gtk_widget_set_no_show_all (image, TRUE);
  gtk_widget_set_size_request (image, DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE);
  gtk_widget_set_margin_bottom (image, 10);
  gtk_container_add (GTK_CONTAINER (box), image);
  pixbuf = frame_pixbuf (NULL, NULL, DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE);
  gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
  g_object_unref (pixbuf);

  spinner = gtk_spinner_new ();
  gtk_widget_set_size_request (spinner, DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE);
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_container_add (GTK_CONTAINER (box), spinner);
  gtk_widget_show (spinner);

  entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
  gtk_box_pack_start (GTK_BOX (box), entry, FALSE, FALSE, 0);

  escaped_address = g_markup_escape_text (ephy_web_view_get_display_address (view), -1);
  markup = g_strdup_printf ("<small>%s</small>", escaped_address);
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), markup);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_max_width_chars (GTK_LABEL (label), 40);
  g_free (markup);
  g_free (escaped_address);

  gtk_box_pack_end (GTK_BOX (box), label, FALSE, FALSE, 0);
  context = gtk_widget_get_style_context (label);
  gtk_style_context_add_class (context, "dim-label");

  data = g_slice_new0 (EphyApplicationDialogData);
  data->view = view;
  data->image = image;
  data->entry = entry;
  data->spinner = spinner;

  g_object_bind_property (image, "visible", spinner, "visible", G_BINDING_INVERT_BOOLEAN);

  fill_default_application_image (data);
  fill_default_application_title (data);

  gtk_widget_show_all (dialog);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  g_signal_connect (dialog, "response",
                    G_CALLBACK (dialog_save_as_application_response_cb),
                    data);
  gtk_widget_show_all (dialog);
}

static char *
get_suggested_filename (EphyEmbed *embed)
{
  EphyWebView *view;
  char *suggested_filename = NULL;
  const char *mimetype;
  WebKitURIResponse *response;
  WebKitWebResource *web_resource;

  view = ephy_embed_get_web_view (embed);
  web_resource = webkit_web_view_get_main_resource (WEBKIT_WEB_VIEW (view));
  response = webkit_web_resource_get_response (web_resource);
  mimetype = webkit_uri_response_get_mime_type (response);

  if ((g_ascii_strncasecmp (mimetype, "text/html", 9)) == 0) {
    /* Web Title will be used as suggested filename */
    suggested_filename = g_strconcat (ephy_embed_get_title (embed), ".mhtml", NULL);
  } else {
    suggested_filename = g_strdup (webkit_uri_response_get_suggested_filename (response));
    if (!suggested_filename) {
      SoupURI *soup_uri = soup_uri_new (webkit_web_resource_get_uri (web_resource));
      char *last_slash = strrchr (soup_uri->path, '/');
      suggested_filename = soup_uri_decode (last_slash ? (last_slash + 1) : soup_uri->path);
      soup_uri_free (soup_uri);
    }
  }

  return suggested_filename;
}

static void
save_response_cb (GtkNativeDialog *dialog, int response, EphyEmbed *embed)
{
  if (response == GTK_RESPONSE_ACCEPT) {
    char *uri, *converted;

    uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
    if (uri != NULL) {
      converted = g_filename_to_utf8 (uri, -1, NULL, NULL, NULL);

      if (converted != NULL) {
        EphyWebView *web_view = ephy_embed_get_web_view (embed);
        ephy_web_view_save (web_view, converted);
      }

      g_free (converted);
      g_free (uri);
    }
  }

  g_object_unref (dialog);
}

void
window_cmd_save_as (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyEmbed *embed;
  GtkFileChooser *dialog;
  char *suggested_filename;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_return_if_fail (embed != NULL);

  dialog = ephy_create_file_chooser (_("Save"),
                                     GTK_WIDGET (window),
                                     GTK_FILE_CHOOSER_ACTION_SAVE,
                                     EPHY_FILE_FILTER_NONE);

  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

  suggested_filename = ephy_sanitize_filename (get_suggested_filename (embed));

  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), suggested_filename);
  g_free (suggested_filename);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (save_response_cb), embed);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));
}

void
window_cmd_undo (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  EphyWindow *window = user_data;
  GtkWidget *widget;
  GtkWidget *embed;
  GtkWidget *location_entry;

  widget = gtk_window_get_focus (GTK_WINDOW (window));
  location_entry = gtk_widget_get_ancestor (widget, EPHY_TYPE_LOCATION_ENTRY);

  if (location_entry) {
    ephy_location_entry_reset (EPHY_LOCATION_ENTRY (location_entry));
  } else {
    embed = gtk_widget_get_ancestor (widget, EPHY_TYPE_EMBED);

    if (embed) {
      webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (EPHY_EMBED (embed)), "Undo");
    }
  }
}

void
window_cmd_redo (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  EphyWindow *window = user_data;
  GtkWidget *widget;
  GtkWidget *embed;
  GtkWidget *location_entry;

  widget = gtk_window_get_focus (GTK_WINDOW (window));
  location_entry = gtk_widget_get_ancestor (widget, EPHY_TYPE_LOCATION_ENTRY);

  if (location_entry) {
    ephy_location_entry_undo_reset (EPHY_LOCATION_ENTRY (location_entry));
  } else {
    embed = gtk_widget_get_ancestor (widget, EPHY_TYPE_EMBED);
    if (embed) {
      webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (EPHY_EMBED (embed)), "Redo");
    }
  }
}
void
window_cmd_cut (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  EphyWindow *window = user_data;
  GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

  if (GTK_IS_EDITABLE (widget)) {
    gtk_editable_cut_clipboard (GTK_EDITABLE (widget));
  } else {
    EphyEmbed *embed;
    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
    g_return_if_fail (embed != NULL);

    webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed), WEBKIT_EDITING_COMMAND_CUT);
  }
}

void
window_cmd_copy (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  EphyWindow *window = user_data;
  GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

  if (GTK_IS_EDITABLE (widget)) {
    gtk_editable_copy_clipboard (GTK_EDITABLE (widget));
  } else {
    EphyEmbed *embed;

    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
    g_return_if_fail (embed != NULL);

    webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed), WEBKIT_EDITING_COMMAND_COPY);
  }
}

void
window_cmd_paste (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  EphyWindow *window = user_data;
  GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

  if (GTK_IS_EDITABLE (widget)) {
    gtk_editable_paste_clipboard (GTK_EDITABLE (widget));
  } else {
    EphyEmbed *embed;

    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
    g_return_if_fail (embed != NULL);

    webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed), WEBKIT_EDITING_COMMAND_PASTE);
  }
}

void
window_cmd_delete (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  EphyWindow *window = user_data;
  GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

  if (GTK_IS_EDITABLE (widget)) {
    gtk_editable_delete_text (GTK_EDITABLE (widget), 0, -1);
  } else {
    EphyEmbed *embed;

    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
    g_return_if_fail (embed != NULL);

    /* FIXME: TODO */
#if 0
    ephy_command_manager_do_command (EPHY_COMMAND_MANAGER (embed),
                                     "cmd_delete");
#endif
  }
}

void
window_cmd_print (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyEmbed *embed;
  EphyWebView *view;

  embed = ephy_embed_container_get_active_child
            (EPHY_EMBED_CONTAINER (window));
  g_return_if_fail (EPHY_IS_EMBED (embed));
  view = ephy_embed_get_web_view (embed);

  ephy_web_view_print (view);
}

void
window_cmd_find (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyFindToolbar *toolbar;

  toolbar = EPHY_FIND_TOOLBAR (ephy_window_get_current_find_toolbar (window));
  ephy_find_toolbar_toggle_state (toolbar);
}

void
window_cmd_find_prev (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyFindToolbar *toolbar;

  toolbar = EPHY_FIND_TOOLBAR (ephy_window_get_current_find_toolbar (window));
  ephy_find_toolbar_find_previous (toolbar);
}

void
window_cmd_find_next (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyFindToolbar *toolbar;

  toolbar = EPHY_FIND_TOOLBAR (ephy_window_get_current_find_toolbar (window));
  ephy_find_toolbar_find_next (toolbar);
}

void
window_cmd_open_bookmark (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       user_data)
{
  const gchar *address;
  EphyLinkFlags flags;

  address = g_variant_get_string (parameter, NULL);
  flags = ephy_link_flags_from_current_event () | EPHY_LINK_BOOKMARK;

  ephy_link_open (EPHY_LINK (user_data), address, NULL, flags);
}

void
window_cmd_bookmark_page (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyHeaderBar *header_bar;
  EphyTitleWidget *title_widget;
  GtkPopover *popover;

  header_bar = EPHY_HEADER_BAR (ephy_window_get_header_bar (window));
  title_widget = ephy_header_bar_get_title_widget (header_bar);
  g_assert (EPHY_IS_LOCATION_ENTRY (title_widget));
  popover = ephy_location_entry_get_add_bookmark_popover (EPHY_LOCATION_ENTRY (title_widget));

  ephy_add_bookmark_popover_show (EPHY_ADD_BOOKMARK_POPOVER (popover));
}

void
window_cmd_zoom_in (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  EphyWindow *window = user_data;

  ephy_window_set_zoom (window, ZOOM_IN);
}

void
window_cmd_zoom_out (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  EphyWindow *window = user_data;

  ephy_window_set_zoom (window, ZOOM_OUT);
}

void
window_cmd_zoom_normal (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  EphyWindow *window = user_data;
  ephy_window_set_zoom (window, 1.0);
}

void
window_cmd_encoding (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyEncodingDialog *dialog;

  dialog = ephy_encoding_dialog_new (window);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
  gtk_dialog_run (GTK_DIALOG (dialog));
}

static void
view_source_embedded (const char *uri, EphyEmbed *embed)
{
  EphyEmbed *new_embed;

  new_embed = ephy_shell_new_tab
                (ephy_shell_get_default (),
                EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
                embed,
                EPHY_NEW_TAB_JUMP | EPHY_NEW_TAB_APPEND_AFTER);

  /* FIXME: Implement embedded view source mode using a custom URI handler and a
   * javascript library for the syntax highlighting.
   * https://bugzilla.gnome.org/show_bug.cgi?id=731558
   */
  webkit_web_view_load_uri
    (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (new_embed), uri);
  gtk_widget_grab_focus (GTK_WIDGET (new_embed));
}

static void
save_temp_source_close_cb (GOutputStream *ostream, GAsyncResult *result, gpointer data)
{
  const char *uri;
  GFile *file;
  GError *error = NULL;

  g_output_stream_close_finish (ostream, result, &error);
  if (error) {
    g_warning ("Unable to close file: %s", error->message);
    g_error_free (error);
    return;
  }

  uri = (const char *)g_object_get_data (G_OBJECT (ostream), "ephy-save-temp-source-uri");

  file = g_file_new_for_uri (uri);

  if (!ephy_file_launch_handler ("text/plain", file, gtk_get_current_event_time ())) {
    /* Fallback to view the source inside the browser */
    EphyEmbed *embed;

    uri = (const char *)g_object_get_data (G_OBJECT (ostream),
                                           "ephy-original-source-uri");
    embed = (EphyEmbed *)g_object_get_data (G_OBJECT (ostream),
                                            "ephy-save-temp-source-embed");
    view_source_embedded (uri, embed);
  }
  g_object_unref (ostream);

  g_object_unref (file);
}

static void
save_temp_source_write_cb (GOutputStream *ostream, GAsyncResult *result, GString *data)
{
  GError *error = NULL;
  gssize written;

  written = g_output_stream_write_finish (ostream, result, &error);
  if (error) {
    g_string_free (data, TRUE);
    g_warning ("Unable to write to file: %s", error->message);
    g_error_free (error);

    g_output_stream_close_async (ostream, G_PRIORITY_DEFAULT, NULL,
                                 (GAsyncReadyCallback)save_temp_source_close_cb,
                                 NULL);

    return;
  }

  if (written == (gint)data->len) {
    g_string_free (data, TRUE);

    g_output_stream_close_async (ostream, G_PRIORITY_DEFAULT, NULL,
                                 (GAsyncReadyCallback)save_temp_source_close_cb,
                                 NULL);

    return;
  }

  data->len -= written;
  data->str += written;

  g_output_stream_write_async (ostream,
                               data->str, data->len,
                               G_PRIORITY_DEFAULT, NULL,
                               (GAsyncReadyCallback)save_temp_source_write_cb,
                               data);
}

static void
get_main_resource_data_cb (WebKitWebResource *resource, GAsyncResult *result, GOutputStream *ostream)
{
  guchar *data;
  gsize data_length;
  GString *data_str;
  GError *error = NULL;

  data = webkit_web_resource_get_data_finish (resource, result, &data_length, &error);
  if (error) {
    g_warning ("Unable to get main resource data: %s", error->message);
    g_error_free (error);
    return;
  }

  /* We create a new GString here because we need to make sure
   * we keep writing in case of partial writes */
  data_str = g_string_new_len ((gchar *)data, data_length);
  g_free (data);

  g_output_stream_write_async (ostream,
                               data_str->str, data_str->len,
                               G_PRIORITY_DEFAULT, NULL,
                               (GAsyncReadyCallback)save_temp_source_write_cb,
                               data_str);
}

static void
save_temp_source_replace_cb (GFile *file, GAsyncResult *result, EphyEmbed *embed)
{
  EphyWebView *view;
  WebKitWebResource *resource;
  GFileOutputStream *ostream;
  GError *error = NULL;

  ostream = g_file_replace_finish (file, result, &error);
  if (error) {
    g_warning ("Unable to replace file: %s", error->message);
    g_error_free (error);
    return;
  }

  g_object_set_data_full (G_OBJECT (ostream),
                          "ephy-save-temp-source-uri",
                          g_file_get_uri (file),
                          g_free);

  view = ephy_embed_get_web_view (embed);

  g_object_set_data_full (G_OBJECT (ostream),
                          "ephy-original-source-uri",
                          g_strdup (webkit_web_view_get_uri (WEBKIT_WEB_VIEW (view))),
                          g_free),

  g_object_set_data_full (G_OBJECT (ostream),
                          "ephy-save-temp-source-embed",
                          g_object_ref (embed),
                          g_object_unref);

  resource = webkit_web_view_get_main_resource (WEBKIT_WEB_VIEW (view));
  webkit_web_resource_get_data (resource, NULL,
                                (GAsyncReadyCallback)get_main_resource_data_cb,
                                ostream);
}

static void
save_temp_source (EphyEmbed *embed,
                  guint32    user_time)
{
  GFile *file;
  char *tmp, *base;
  const char *static_temp_dir;

  static_temp_dir = ephy_file_tmp_dir ();
  if (static_temp_dir == NULL) {
    return;
  }

  base = g_build_filename (static_temp_dir, "viewsourceXXXXXX", NULL);
  tmp = ephy_file_tmp_filename (base, "html");
  g_free (base);
  if (tmp == NULL) {
    return;
  }

  file = g_file_new_for_path (tmp);
  g_file_replace_async (file, NULL, FALSE,
                        G_FILE_CREATE_REPLACE_DESTINATION | G_FILE_CREATE_PRIVATE,
                        G_PRIORITY_DEFAULT, NULL,
                        (GAsyncReadyCallback)save_temp_source_replace_cb,
                        embed);

  g_object_unref (file);
  g_free (tmp);
}

void
window_cmd_page_source (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyEmbed *embed;
  const char *address;
  guint32 user_time;

  embed = ephy_embed_container_get_active_child
            (EPHY_EMBED_CONTAINER (window));
  g_return_if_fail (embed != NULL);

  address = ephy_web_view_get_address (ephy_embed_get_web_view (embed));

#if 0
 FIXME: Disabled due to bug #738475

  if (g_settings_get_boolean (EPHY_SETTINGS_MAIN,
                              EPHY_PREFS_INTERNAL_VIEW_SOURCE)) {
    view_source_embedded (address, embed);
    return;
  }
#endif

  user_time = gtk_get_current_event_time ();

  if (g_str_has_prefix (address, "file://")) {
    GFile *file;

    file = g_file_new_for_uri (address);
    ephy_file_launch_handler ("text/plain", file, user_time);

    g_object_unref (file);
  } else {
    save_temp_source (embed, user_time);
  }
}

void
window_cmd_toggle_inspector (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyEmbed *embed;
  WebKitWebView *view;
  WebKitWebInspector *inspector_window;

  embed = ephy_embed_container_get_active_child
            (EPHY_EMBED_CONTAINER (window));
  g_return_if_fail (embed != NULL);

  gtk_widget_grab_focus (GTK_WIDGET (embed));

  view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  inspector_window = webkit_web_view_get_inspector (view);

  if (!ephy_embed_inspector_is_loaded (embed))
    webkit_web_inspector_show (inspector_window);
  else
    webkit_web_inspector_close (inspector_window);
}

void
window_cmd_select_all (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  EphyWindow *window = user_data;

  GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

  if (GTK_IS_EDITABLE (widget)) {
    gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);
  } else {
    EphyEmbed *embed;

    embed = ephy_embed_container_get_active_child
              (EPHY_EMBED_CONTAINER (window));
    g_return_if_fail (embed != NULL);

    webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed), "SelectAll");
  }
}

void
window_cmd_send_to (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyEmbed *embed;
  char *command, *subject, *body;
  const char *location, *title;
  GError *error = NULL;

  embed = ephy_embed_container_get_active_child
            (EPHY_EMBED_CONTAINER (window));
  g_return_if_fail (embed != NULL);

  location = ephy_web_view_get_address (ephy_embed_get_web_view (embed));
  title = ephy_embed_get_title (embed);

  subject = g_uri_escape_string (title, NULL, TRUE);
  body = g_uri_escape_string (location, NULL, TRUE);

  command = g_strconcat ("mailto:",
                         "?Subject=", subject,
                         "&Body=", body, NULL);

  g_free (subject);
  g_free (body);

  if (!gtk_show_uri_on_window (GTK_WINDOW (window), command, gtk_get_current_event_time (), &error)) {
    g_warning ("Unable to send link by email: %s\n", error->message);
    g_error_free (error);
  }

  g_free (command);
}

void
window_cmd_go_location (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  ephy_window_activate_location (user_data);
}

void
window_cmd_go_home (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  ephy_link_open (EPHY_LINK (user_data),
                  NULL, NULL,
                  EPHY_LINK_HOME_PAGE);
}

void
window_cmd_change_browse_with_caret_state (GSimpleAction *action,
                                           GVariant      *state,
                                           gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  gboolean active;

  active = g_variant_get_boolean (state);

  if (active) {
    GtkWidget *dialog;
    int response;

    dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL,
                                     _("Enable caret browsing mode?"));

    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              _("Pressing F7 turns caret browsing on or off. This feature "
                                                "places a moveable cursor in web pages, allowing you to move "
                                                "around with your keyboard. Do you want to enable caret browsing?"));
    gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Enable"), GTK_RESPONSE_ACCEPT);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

    response = gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_destroy (dialog);

    if (response == GTK_RESPONSE_CANCEL) {
      g_simple_action_set_state (action, g_variant_new_boolean (FALSE));
      return;
    }
  }

  g_simple_action_set_state (action, g_variant_new_boolean (active));
  g_settings_set_boolean (EPHY_SETTINGS_MAIN,
                          EPHY_PREFS_ENABLE_CARET_BROWSING, active);
}

void
window_cmd_change_fullscreen_state (GSimpleAction *action,
                                    GVariant      *state,
                                    gpointer       user_data)
{
  gboolean active;

  active = g_variant_get_boolean (state);
  if (active)
    gtk_window_fullscreen (GTK_WINDOW (user_data));
  else
    gtk_window_unfullscreen (GTK_WINDOW (user_data));

  g_simple_action_set_state (action, g_variant_new_boolean (active));
}

void
window_cmd_tabs_previous (GSimpleAction *action,
                          GVariant      *variant,
                          gpointer       user_data)
{
  GtkWidget *nb;

  nb = ephy_window_get_notebook (EPHY_WINDOW (user_data));
  g_return_if_fail (nb != NULL);

  ephy_notebook_prev_page (EPHY_NOTEBOOK (nb));
}

void
window_cmd_tabs_next (GSimpleAction *action,
                      GVariant      *variant,
                      gpointer       user_data)
{
  GtkWidget *nb;

  nb = ephy_window_get_notebook (EPHY_WINDOW (user_data));
  g_return_if_fail (nb != NULL);

  ephy_notebook_next_page (EPHY_NOTEBOOK (nb));
}

void
window_cmd_tabs_move_left (GSimpleAction *action,
                           GVariant      *variant,
                           gpointer       user_data)
{
  GtkWidget *child;
  GtkNotebook *notebook;
  int page;

  notebook = GTK_NOTEBOOK (ephy_window_get_notebook (EPHY_WINDOW (user_data)));
  page = gtk_notebook_get_current_page (notebook);
  if (page < 1)
    return;

  child = gtk_notebook_get_nth_page (notebook, page);
  gtk_notebook_reorder_child (notebook, child, page - 1);
}

void window_cmd_tabs_move_right (GSimpleAction *action,
                                 GVariant      *variant,
                                 gpointer       user_data)
{
  GtkWidget *child;
  GtkNotebook *notebook;
  int page, n_pages;

  notebook = GTK_NOTEBOOK (ephy_window_get_notebook (EPHY_WINDOW (user_data)));
  page = gtk_notebook_get_current_page (notebook);
  n_pages = gtk_notebook_get_n_pages (notebook) - 1;
  if (page > n_pages - 1)
    return;

  child = gtk_notebook_get_nth_page (notebook, page);
  gtk_notebook_reorder_child (notebook, child, page + 1);
}

void
window_cmd_tabs_duplicate (GSimpleAction *action,
                           GVariant      *variant,
                           gpointer       user_data)
{
  EphyEmbed *embed, *new_embed;
  WebKitWebView *view, *new_view;
  WebKitWebViewSessionState *session_state;
  WebKitBackForwardList *bf_list;
  WebKitBackForwardListItem *item;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (user_data));
  view = WEBKIT_WEB_VIEW (ephy_embed_get_web_view (embed));
  session_state = webkit_web_view_get_session_state (view);

  new_embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  EPHY_WINDOW (user_data),
                                  embed,
                                  EPHY_NEW_TAB_APPEND_AFTER | EPHY_NEW_TAB_JUMP);

  new_view = WEBKIT_WEB_VIEW (ephy_embed_get_web_view (new_embed));

  webkit_web_view_restore_session_state (new_view, session_state);
  webkit_web_view_session_state_unref (session_state);

  bf_list = webkit_web_view_get_back_forward_list (new_view);
  item = webkit_back_forward_list_get_current_item (bf_list);
  if (item)
    webkit_web_view_go_to_back_forward_list_item (new_view, item);
  else
    ephy_web_view_load_url (EPHY_WEB_VIEW (new_view), webkit_web_view_get_uri (view));
}

void
window_cmd_tabs_detach (GSimpleAction *action,
                        GVariant      *variant,
                        gpointer       user_data)
{
  EphyEmbed *embed;
  GtkNotebook *notebook;
  EphyWindow *new_window;

  notebook = GTK_NOTEBOOK (ephy_window_get_notebook (EPHY_WINDOW (user_data)));
  if (gtk_notebook_get_n_pages (notebook) <= 1)
    return;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (user_data));

  g_object_ref_sink (embed);
  gtk_notebook_remove_page (notebook, gtk_notebook_page_num (notebook, GTK_WIDGET (embed)));

  new_window = ephy_window_new ();
  ephy_embed_container_add_child (EPHY_EMBED_CONTAINER (new_window), embed, 0, FALSE);
  g_object_unref (embed);

  gtk_window_present (GTK_WINDOW (new_window));
}

void
window_cmd_tabs_close (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  EphyWindow *window = user_data;
  GtkWidget *notebook;
  EphyEmbed *embed;

  notebook = ephy_window_get_notebook (window);

  if (g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
                              EPHY_PREFS_LOCKDOWN_QUIT) &&
      gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) == 1) {
    return;
  }

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_return_if_fail (embed != NULL);

  g_signal_emit_by_name (notebook, "tab-close-request", embed);
}

void
window_cmd_show_tab (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  GtkWidget *notebook;
  guint32 tab_num;

  g_assert (g_variant_is_of_type (parameter, G_VARIANT_TYPE_UINT32));
  tab_num = g_variant_get_uint32 (parameter);

  notebook = ephy_window_get_notebook (window);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), tab_num);
  g_simple_action_set_state (action, parameter);
}

void
window_cmd_change_show_tab_state (GSimpleAction *action,
                                  GVariant      *parameter,
                                  gpointer       user_data)
{
  /* This page intentionally left blank. */
}
