/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 200-2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *  Copyright © 2010, 2017 Igalia S.L.
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
#include "prefs-dialog.h"

#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "ephy-flatpak-utils.h"
#include "ephy-gui.h"
#include "ephy-langs.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-prefs.h"
#include "ephy-search-engine-dialog.h"
#include "ephy-session.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-sync-service.h"
#include "ephy-sync-utils.h"
#include "ephy-time-helpers.h"
#include "ephy-uri-tester-shared.h"
#include "ephy-web-app-utils.h"
#include "clear-data-dialog.h"
#include "cookies-dialog.h"
#include "gnome-languages.h"
#include "passwords-dialog.h"
#include "synced-tabs-dialog.h"
#include "webapp-additional-urls-dialog.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#define HANDY_USE_UNSTABLE_API
#include <handy.h>
#include <JavaScriptCore/JavaScript.h>
#include <json-glib/json-glib.h>
#include <math.h>
#include <string.h>

#define DOWNLOAD_BUTTON_WIDTH   8
#define FXA_IFRAME_URL "https://accounts.firefox.com/signin?service=sync&context=fx_desktop_v3"

enum {
  COL_LANG_NAME,
  COL_LANG_CODE
};

struct _PrefsDialog {
  HdyDialog parent_instance;

  GtkWidget *notebook;

  /* general */
  GtkWidget *homepage_box;
  GtkWidget *homepage_list;
  GtkWidget *new_tab_homepage_radiobutton;
  GtkWidget *blank_homepage_radiobutton;
  GtkWidget *custom_homepage_radiobutton;
  GtkWidget *custom_homepage_entry;
  EphyWebApplication *webapp;
  guint webapp_save_id;
  GtkWidget *webapp_box;
  GtkWidget *webapp_list;
  GtkWidget *webapp_icon;
  GtkWidget *webapp_url;
  GtkWidget *webapp_title;
  GtkWidget *downloads_list;
  GtkWidget *download_folder_row;
  GtkWidget *download_box;
  GtkWidget *search_box;
  GtkWidget *search_engines_list;
  GtkWidget *session_box;
  GtkWidget *browsing_box;
  GtkWidget *browsing_list;
  GtkWidget *session_list;
  GtkWidget *restore_session_switch;
  GtkWidget *popups_allow_switch;
  GtkWidget *web_content_list;
  GtkWidget *adblock_allow_switch;
  GtkWidget *enable_plugins_checkbutton;
  GtkWidget *enable_safe_browsing_switch;
  GtkWidget *enable_smooth_scrolling_switch;

  /* fonts & style */
  GtkWidget *fonts_list;
  GtkWidget *use_gnome_fonts_row;
  GtkWidget *use_custom_fonts_list;
  GtkWidget *sans_fontbutton;
  GtkWidget *serif_fontbutton;
  GtkWidget *mono_fontbutton;
  GtkWidget *css_list;
  GtkWidget *css_switch;
  GtkWidget *css_edit_button;
  GtkWidget *default_zoom_spin_button;
  GtkWidget *reader_mode_box;
  GtkWidget *reader_mode_list;
  GtkWidget *reader_mode_font_style;
  GtkWidget *reader_mode_color_scheme;

  /* stored data */
  GtkWidget *cookies_list;
  GtkWidget *always;
  GtkWidget *no_third_party;
  GtkWidget *never;
  GtkWidget *passwords_list;
  GtkWidget *remember_passwords_switch;
  GtkWidget *personal_data_list;
  GtkWidget *do_not_track_row;
  GtkWidget *do_not_track_switch;
  GtkWidget *clear_personal_data_button;

  /* language */
  GtkTreeView *lang_treeview;
  GtkWidget *lang_add_button;
  GtkWidget *lang_remove_button;
  GtkWidget *lang_up_button;
  GtkWidget *lang_down_button;
  GtkWidget *spell_checking_list;
  GtkWidget *enable_spell_checking_switch;

  GtkDialog *add_lang_dialog;
  GtkTreeView *add_lang_treeview;
  GtkTreeModel *lang_model;

  /* sync */
  GtkWidget *sync_page_box;
  GtkWidget *sync_firefox_iframe_box;
  GtkWidget *sync_firefox_iframe_label;
  GtkWidget *sync_firefox_account_box;
  GtkWidget *sync_firefox_account_label;
  GtkWidget *sync_sign_out_button;
  GtkWidget *sync_options_box;
  GtkWidget *sync_bookmarks_checkbutton;
  GtkWidget *sync_passwords_checkbutton;
  GtkWidget *sync_history_checkbutton;
  GtkWidget *sync_open_tabs_checkbutton;
  GtkWidget *sync_frequency_5_min_radiobutton;
  GtkWidget *sync_frequency_15_min_radiobutton;
  GtkWidget *sync_frequency_30_min_radiobutton;
  GtkWidget *sync_frequency_60_min_radiobutton;
  GtkWidget *sync_now_button;
  GtkWidget *synced_tabs_button;
  GtkWidget *sync_device_name_entry;
  GtkWidget *sync_device_name_change_button;
  GtkWidget *sync_device_name_save_button;
  GtkWidget *sync_device_name_cancel_button;
  GtkWidget *sync_last_sync_time_box;
  GtkWidget *sync_last_sync_time_label;

  WebKitWebView *fxa_web_view;
  WebKitUserContentManager *fxa_manager;
  WebKitUserScript *fxa_script;
};

enum {
  COL_TITLE_ELIDED,
  COL_ENCODING,
  NUM_COLS
};

G_DEFINE_TYPE (PrefsDialog, prefs_dialog, HDY_TYPE_DIALOG)

static void
prefs_dialog_finalize (GObject *object)
{
  PrefsDialog *dialog = EPHY_PREFS_DIALOG (object);

  if (dialog->add_lang_dialog != NULL) {
    GtkDialog **add_lang_dialog = &dialog->add_lang_dialog;

    g_object_remove_weak_pointer (G_OBJECT (dialog->add_lang_dialog),
                                  (gpointer *)add_lang_dialog);
    g_object_unref (dialog->add_lang_dialog);
  }

  if (dialog->fxa_web_view != NULL) {
    webkit_user_content_manager_unregister_script_message_handler (dialog->fxa_manager,
                                                                   "toChromeMessageHandler");
    webkit_user_content_manager_unregister_script_message_handler (dialog->fxa_manager,
                                                                   "openWebmailClickHandler");
    webkit_user_script_unref (dialog->fxa_script);
    g_object_unref (dialog->fxa_manager);
  }

  g_clear_pointer (&dialog->webapp, ephy_web_application_free);

  G_OBJECT_CLASS (prefs_dialog_parent_class)->finalize (object);
}

static void
sync_collection_toggled_cb (GtkToggleButton *button,
                            PrefsDialog     *dialog)
{
  EphySynchronizableManager *manager = NULL;
  EphyShell *shell = ephy_shell_get_default ();
  EphySyncService *service = ephy_shell_get_sync_service (shell);

  if (GTK_WIDGET (button) == dialog->sync_bookmarks_checkbutton) {
    manager = EPHY_SYNCHRONIZABLE_MANAGER (ephy_shell_get_bookmarks_manager (shell));
  } else if (GTK_WIDGET (button) == dialog->sync_passwords_checkbutton) {
    manager = EPHY_SYNCHRONIZABLE_MANAGER (ephy_embed_shell_get_password_manager (EPHY_EMBED_SHELL (shell)));
  } else if (GTK_WIDGET (button) == dialog->sync_history_checkbutton) {
    manager = EPHY_SYNCHRONIZABLE_MANAGER (ephy_shell_get_history_manager (shell));
  } else if (GTK_WIDGET (button) == dialog->sync_open_tabs_checkbutton) {
    manager = EPHY_SYNCHRONIZABLE_MANAGER (ephy_shell_get_open_tabs_manager (shell));
    ephy_open_tabs_manager_clear_cache (EPHY_OPEN_TABS_MANAGER (manager));
  } else {
    g_assert_not_reached ();
  }

  if (gtk_toggle_button_get_active (button)) {
    ephy_sync_service_register_manager (service, manager);
  } else {
    ephy_sync_service_unregister_manager (service, manager);
    ephy_synchronizable_manager_set_is_initial_sync (manager, TRUE);
  }
}

static void
sync_set_last_sync_time (PrefsDialog *dialog)
{
  gint64 sync_time = ephy_sync_utils_get_sync_time ();

  if (sync_time) {
    char *time = ephy_time_helpers_utf_friendly_time (sync_time);
    /* Translators: the %s refers to the time at which the last sync was made.
     * For example: Today 04:34 PM, Sun 11:25 AM, May 31 06:41 PM.
     */
    char *text = g_strdup_printf (_("Last synchronized: %s"), time);

    gtk_label_set_text (GTK_LABEL (dialog->sync_last_sync_time_label), text);
    gtk_widget_set_visible (dialog->sync_last_sync_time_box, TRUE);

    g_free (text);
    g_free (time);
  }
}

static void
sync_finished_cb (EphySyncService *service,
                  PrefsDialog     *dialog)
{
  g_assert (EPHY_IS_SYNC_SERVICE (service));
  g_assert (EPHY_IS_PREFS_DIALOG (dialog));

  gtk_widget_set_sensitive (dialog->sync_now_button, TRUE);
  sync_set_last_sync_time (dialog);
}

static void
sync_sign_in_details_show (PrefsDialog *dialog,
                           const char  *text)
{
  char *message;

  g_assert (EPHY_IS_PREFS_DIALOG (dialog));

  message = g_strdup_printf ("<span fgcolor='#e6780b'>%s</span>", text);
  gtk_label_set_markup (GTK_LABEL (dialog->sync_firefox_iframe_label), message);
  gtk_widget_set_visible (dialog->sync_firefox_iframe_label, TRUE);

  g_free (message);
}

static void
sync_sign_in_error_cb (EphySyncService *service,
                       const char      *error,
                       PrefsDialog     *dialog)
{
  g_assert (EPHY_IS_SYNC_SERVICE (service));
  g_assert (EPHY_IS_PREFS_DIALOG (dialog));

  /* Display the error message and reload the iframe. */
  sync_sign_in_details_show (dialog, error);
  webkit_web_view_load_uri (dialog->fxa_web_view, FXA_IFRAME_URL);
}

static void
sync_secrets_store_finished_cb (EphySyncService *service,
                                GError          *error,
                                PrefsDialog     *dialog)
{
  g_assert (EPHY_IS_SYNC_SERVICE (service));
  g_assert (EPHY_IS_PREFS_DIALOG (dialog));

  if (!error) {
    /* Show sync options panel. */
    char *user = g_strdup_printf ("<b>%s</b>", ephy_sync_utils_get_sync_user ());
    /* Translators: the %s refers to the email of the currently logged in user. */
    char *text = g_strdup_printf (_("Logged in as %s"), user);

    gtk_label_set_markup (GTK_LABEL (dialog->sync_firefox_account_label), text);
    gtk_container_remove (GTK_CONTAINER (dialog->sync_page_box),
                          dialog->sync_firefox_iframe_box);
    gtk_box_pack_start (GTK_BOX (dialog->sync_page_box),
                        dialog->sync_firefox_account_box,
                        FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (dialog->sync_page_box),
                        dialog->sync_options_box,
                        FALSE, FALSE, 0);

    g_free (text);
    g_free (user);
  } else {
    /* Display the error message and reload the iframe. */
    sync_sign_in_details_show (dialog, error->message);
    webkit_web_view_load_uri (dialog->fxa_web_view, FXA_IFRAME_URL);
  }
}

static void
sync_message_to_fxa_content (PrefsDialog *dialog,
                             const char  *web_channel_id,
                             const char  *command,
                             const char  *message_id,
                             JsonObject  *data)
{
  JsonNode *node;
  JsonObject *detail;
  JsonObject *message;
  char *detail_str;
  char *script;
  const char *type;

  g_assert (EPHY_IS_PREFS_DIALOG (dialog));
  g_assert (web_channel_id);
  g_assert (command);
  g_assert (message_id);
  g_assert (data);

  message = json_object_new ();
  json_object_set_string_member (message, "command", command);
  json_object_set_string_member (message, "messageId", message_id);
  json_object_set_object_member (message, "data", json_object_ref (data));
  detail = json_object_new ();
  json_object_set_string_member (detail, "id", web_channel_id);
  json_object_set_object_member (detail, "message", message);
  node = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (node, detail);

  type = "WebChannelMessageToContent";
  detail_str = json_to_string (node, FALSE);
  script = g_strdup_printf ("let e = new window.CustomEvent(\"%s\", {detail: %s});"
                            "window.dispatchEvent(e);",
                            type, detail_str);

  /* We don't expect any response from the server. */
  webkit_web_view_run_javascript (dialog->fxa_web_view, script, NULL, NULL, NULL);

  g_free (script);
  g_free (detail_str);
  json_object_unref (detail);
  json_node_unref (node);
}

static gboolean
sync_parse_message_from_fxa_content (const char  *message,
                                     char       **web_channel_id,
                                     char       **message_id,
                                     char       **command,
                                     JsonObject **data,
                                     char       **error_msg)
{
  JsonNode *node;
  JsonObject *object;
  JsonObject *detail;
  JsonObject *msg;
  JsonObject *msg_data = NULL;
  const char *type;
  const char *channel_id;
  const char *cmd;
  const char *error = NULL;
  gboolean success = FALSE;

  g_assert (message);
  g_assert (web_channel_id);
  g_assert (message_id);
  g_assert (command);
  g_assert (data);
  g_assert (error_msg);

  /* Expected message format is:
   * {
   *   type: "WebChannelMessageToChrome",
   *   detail: {
   *     id: <id> (string, the id of the WebChannel),
   *     message: {
   *       messageId: <messageId> (optional string, the message id),
   *       command: <command> (string, the message command),
   *       data: <data> (optional JSON object, the message data)
   *     }
   *   }
   * }
   */

  node = json_from_string (message, NULL);
  if (!node) {
    error = "Message is not a valid JSON";
    goto out_error;
  }
  object = json_node_get_object (node);
  if (!object) {
    error = "Message is not a JSON object";
    goto out_error;
  }
  type = json_object_get_string_member (object, "type");
  if (!type) {
    error = "Message has missing or invalid 'type' member";
    goto out_error;
  } else if (strcmp (type, "WebChannelMessageToChrome")) {
    error = "Message type is not WebChannelMessageToChrome";
    goto out_error;
  }
  detail = json_object_get_object_member (object, "detail");
  if (!detail) {
    error = "Message has missing or invalid 'detail' member";
    goto out_error;
  }
  channel_id = json_object_get_string_member (detail, "id");
  if (!channel_id) {
    error = "'Detail' object has missing or invalid 'id' member";
    goto out_error;
  }
  msg = json_object_get_object_member (detail, "message");
  if (!msg) {
    error = "'Detail' object has missing or invalid 'message' member";
    goto out_error;
  }
  cmd = json_object_get_string_member (msg, "command");
  if (!cmd) {
    error = "'Message' object has missing or invalid 'command' member";
    goto out_error;
  }

  *web_channel_id = g_strdup (channel_id);
  *command = g_strdup (cmd);
  *message_id = json_object_has_member (msg, "messageId") ?
                g_strdup (json_object_get_string_member (msg, "messageId")) :
                NULL;
  if (json_object_has_member (msg, "data"))
    msg_data = json_object_get_object_member (msg, "data");
  *data = msg_data ? json_object_ref (msg_data) : NULL;

  success = TRUE;
  *error_msg = NULL;
  goto out_no_error;

out_error:
  *web_channel_id = NULL;
  *command = NULL;
  *message_id = NULL;
  *error_msg = g_strdup (error);

out_no_error:
  json_node_unref (node);

  return success;
}

static void
sync_message_from_fxa_content_cb (WebKitUserContentManager *manager,
                                  WebKitJavascriptResult   *result,
                                  PrefsDialog              *dialog)
{
  JsonObject *data = NULL;
  char *message = NULL;
  char *web_channel_id = NULL;
  char *message_id = NULL;
  char *command = NULL;
  char *error_msg = NULL;
  gboolean is_error = FALSE;

  message = jsc_value_to_string (webkit_javascript_result_get_js_value (result));
  if (!message) {
    g_warning ("Failed to get JavaScript result as string");
    is_error = TRUE;
    goto out;
  }

  if (!sync_parse_message_from_fxa_content (message, &web_channel_id,
                                            &message_id, &command,
                                            &data, &error_msg)) {
    g_warning ("Failed to parse message from FxA Content Server: %s", error_msg);
    is_error = TRUE;
    goto out;
  }

  LOG ("WebChannelMessageToChrome: received %s command", command);

  if (!g_strcmp0 (command, "fxaccounts:can_link_account")) {
    /* Confirm a relink. Respond with {ok: true}. */
    JsonObject *response = json_object_new ();
    json_object_set_boolean_member (response, "ok", TRUE);
    sync_message_to_fxa_content (dialog, web_channel_id, command, message_id, response);
    json_object_unref (response);
  } else if (!g_strcmp0 (command, "fxaccounts:login")) {
    /* Extract sync tokens and pass them to the sync service. */
    const char *email = json_object_get_string_member (data, "email");
    const char *uid = json_object_get_string_member (data, "uid");
    const char *session_token = json_object_get_string_member (data, "sessionToken");
    const char *key_fetch_token = json_object_get_string_member (data, "keyFetchToken");
    const char *unwrap_kb = json_object_get_string_member (data, "unwrapBKey");

    if (!email || !uid || !session_token || !key_fetch_token || !unwrap_kb) {
      g_warning ("Message data has missing or invalid members");
      is_error = TRUE;
      goto out;
    }
    if (!json_object_has_member (data, "verified") ||
        !JSON_NODE_HOLDS_VALUE (json_object_get_member (data, "verified"))) {
      g_warning ("Message data has missing or invalid 'verified' member");
      is_error = TRUE;
      goto out;
    }

    ephy_sync_service_sign_in (ephy_shell_get_sync_service (ephy_shell_get_default ()),
                               email, uid, session_token, key_fetch_token, unwrap_kb);
  }

out:
  if (data)
    json_object_unref (data);
  g_free (message);
  g_free (web_channel_id);
  g_free (message_id);
  g_free (command);
  g_free (error_msg);

  if (is_error) {
    sync_sign_in_details_show (dialog, _("Something went wrong, please try again later."));
    webkit_web_view_load_uri (dialog->fxa_web_view, FXA_IFRAME_URL);
  }
}

static void
sync_open_webmail_clicked_cb (WebKitUserContentManager *manager,
                              WebKitJavascriptResult   *result,
                              PrefsDialog              *dialog)
{
  EphyShell *shell;
  EphyEmbed *embed;
  GtkWindow *window;
  char *url;

  url = jsc_value_to_string (webkit_javascript_result_get_js_value (result));
  if (url) {
    /* Open a new tab to the webmail URL. */
    shell = ephy_shell_get_default ();
    window = gtk_application_get_active_window (GTK_APPLICATION (shell));
    embed = ephy_shell_new_tab (shell, EPHY_WINDOW (window),
                                NULL, EPHY_NEW_TAB_JUMP);
    ephy_web_view_load_url (ephy_embed_get_web_view (embed), url);

    /* Close the preferences dialog. */
    gtk_widget_destroy (GTK_WIDGET (dialog));

    g_free (url);
  }
}

static void
sync_setup_firefox_iframe (PrefsDialog *dialog)
{
  EphyEmbedShell *shell;
  WebKitWebsiteDataManager *manager;
  WebKitWebContext *embed_context;
  WebKitWebContext *sync_context;
  const char *script;

  if (!dialog->fxa_web_view) {
    script =
             /* Handle sign-in messages from the FxA content server. */
             "function handleToChromeMessage(event) {"
             "  let e = JSON.stringify({type: event.type, detail: event.detail});"
             "  window.webkit.messageHandlers.toChromeMessageHandler.postMessage(e);"
             "};"
             "window.addEventListener('WebChannelMessageToChrome', handleToChromeMessage);"
             /* Handle open-webmail click event. */
             "function handleOpenWebmailClick(event) {"
             "  if (event.target.id == 'open-webmail' && event.target.hasAttribute('href'))"
             "    window.webkit.messageHandlers.openWebmailClickHandler.postMessage(event.target.getAttribute('href'));"
             "};"
             "var stage = document.getElementById('stage');"
             "if (stage)"
             "  stage.addEventListener('click', handleOpenWebmailClick);";

    dialog->fxa_script = webkit_user_script_new (script,
                                                 WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
                                                 WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END,
                                                 NULL, NULL);
    dialog->fxa_manager = webkit_user_content_manager_new ();
    webkit_user_content_manager_add_script (dialog->fxa_manager, dialog->fxa_script);
    g_signal_connect (dialog->fxa_manager,
                      "script-message-received::toChromeMessageHandler",
                      G_CALLBACK (sync_message_from_fxa_content_cb),
                      dialog);
    g_signal_connect (dialog->fxa_manager,
                      "script-message-received::openWebmailClickHandler",
                      G_CALLBACK (sync_open_webmail_clicked_cb),
                      dialog);
    webkit_user_content_manager_register_script_message_handler (dialog->fxa_manager,
                                                                 "toChromeMessageHandler");
    webkit_user_content_manager_register_script_message_handler (dialog->fxa_manager,
                                                                 "openWebmailClickHandler");

    shell = ephy_embed_shell_get_default ();
    embed_context = ephy_embed_shell_get_web_context (shell);
    manager = webkit_web_context_get_website_data_manager (embed_context);
    sync_context = webkit_web_context_new_with_website_data_manager (manager);
    webkit_web_context_set_preferred_languages (sync_context,
                                                g_object_get_data (G_OBJECT (embed_context), "preferred-languages"));

    dialog->fxa_web_view = WEBKIT_WEB_VIEW (g_object_new (WEBKIT_TYPE_WEB_VIEW,
                                                          "user-content-manager", dialog->fxa_manager,
                                                          "settings", ephy_embed_prefs_get_settings (),
                                                          "web-context", sync_context,
                                                          NULL));
    gtk_widget_set_visible (GTK_WIDGET (dialog->fxa_web_view), TRUE);
    gtk_widget_set_size_request (GTK_WIDGET (dialog->fxa_web_view), 450, 450);
    gtk_box_pack_start (GTK_BOX (dialog->sync_firefox_iframe_box),
                      GTK_WIDGET (dialog->fxa_web_view),
                      FALSE, FALSE, 0);

    g_object_unref (sync_context);
  }

  webkit_web_view_load_uri (dialog->fxa_web_view, FXA_IFRAME_URL);
  gtk_widget_set_visible (dialog->sync_firefox_iframe_label, FALSE);
}

static void
on_sync_sign_out_button_clicked (GtkWidget   *button,
                                 PrefsDialog *dialog)
{
  EphySyncService *service = ephy_shell_get_sync_service (ephy_shell_get_default ());

  ephy_sync_service_sign_out (service);

  /* Show Firefox Accounts iframe. */
  sync_setup_firefox_iframe (dialog);
  gtk_container_remove (GTK_CONTAINER (dialog->sync_page_box),
                        dialog->sync_firefox_account_box);
  gtk_container_remove (GTK_CONTAINER (dialog->sync_page_box),
                        dialog->sync_options_box);
  gtk_box_pack_start (GTK_BOX (dialog->sync_page_box),
                      dialog->sync_firefox_iframe_box,
                      FALSE, FALSE, 0);
  gtk_widget_set_visible (dialog->sync_last_sync_time_box, FALSE);
}

static void
on_sync_sync_now_button_clicked (GtkWidget   *button,
                                 PrefsDialog *dialog)
{
  EphySyncService *service = ephy_shell_get_sync_service (ephy_shell_get_default ());

  gtk_widget_set_sensitive (button, FALSE);
  ephy_sync_service_sync (service);
}

static void
on_sync_synced_tabs_button_clicked (GtkWidget   *button,
                                    PrefsDialog *dialog)
{
  EphyOpenTabsManager *manager;
  SyncedTabsDialog *synced_tabs_dialog;

  manager = ephy_shell_get_open_tabs_manager (ephy_shell_get_default ());
  synced_tabs_dialog = synced_tabs_dialog_new (manager);
  gtk_window_set_transient_for (GTK_WINDOW (synced_tabs_dialog), GTK_WINDOW (dialog));
  gtk_window_set_modal (GTK_WINDOW (synced_tabs_dialog), TRUE);
  gtk_window_present (GTK_WINDOW (synced_tabs_dialog));
}

static void
on_sync_device_name_change_button_clicked (GtkWidget   *button,
                                           PrefsDialog *dialog)
{
  gtk_widget_set_sensitive (GTK_WIDGET (dialog->sync_device_name_entry), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (dialog->sync_device_name_change_button), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (dialog->sync_device_name_save_button), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (dialog->sync_device_name_cancel_button), TRUE);
}

static void
on_sync_device_name_save_button_clicked (GtkWidget   *button,
                                         PrefsDialog *dialog)
{
  EphySyncService *service = ephy_shell_get_sync_service (ephy_shell_get_default ());
  const char *text;

  text = gtk_entry_get_text (GTK_ENTRY (dialog->sync_device_name_entry));
  if (!g_strcmp0 (text, "")) {
    char *name = ephy_sync_utils_get_device_name ();
    gtk_entry_set_text (GTK_ENTRY (dialog->sync_device_name_entry), name);
    g_free (name);
  } else {
    ephy_sync_service_update_device_name (service, text);
  }

  gtk_widget_set_sensitive (GTK_WIDGET (dialog->sync_device_name_entry), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (dialog->sync_device_name_change_button), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (dialog->sync_device_name_save_button), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (dialog->sync_device_name_cancel_button), FALSE);
}

static void
on_sync_device_name_cancel_button_clicked (GtkWidget   *button,
                                           PrefsDialog *dialog)
{
  char *name;

  name = ephy_sync_utils_get_device_name ();
  gtk_entry_set_text (GTK_ENTRY (dialog->sync_device_name_entry), name);

  gtk_widget_set_sensitive (GTK_WIDGET (dialog->sync_device_name_entry), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (dialog->sync_device_name_change_button), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (dialog->sync_device_name_save_button), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (dialog->sync_device_name_cancel_button), FALSE);

  g_free (name);
}

static gboolean
save_web_application (PrefsDialog *dialog)
{
  gboolean changed = FALSE;
  const char *text;

  dialog->webapp_save_id = 0;

  if (!dialog->webapp)
    return G_SOURCE_REMOVE;

  text = gtk_entry_get_text (GTK_ENTRY (dialog->webapp_url));
  if (g_strcmp0 (dialog->webapp->url, text) != 0) {
    g_free (dialog->webapp->url);
    dialog->webapp->url = g_strdup (text);
    changed = TRUE;
  }

  text = gtk_entry_get_text (GTK_ENTRY (dialog->webapp_title));
  if (g_strcmp0 (dialog->webapp->name, text) != 0) {
    g_free (dialog->webapp->name);
    dialog->webapp->name = g_strdup (text);
    changed = TRUE;
  }

  text = (const char *)g_object_get_data (G_OBJECT (dialog->webapp_icon), "ephy-webapp-icon-url");
  if (g_strcmp0 (dialog->webapp->icon_url, text) != 0) {
    g_free (dialog->webapp->icon_url);
    dialog->webapp->icon_url = g_strdup (text);
    changed = TRUE;
  }

  if (changed)
    ephy_web_application_save (dialog->webapp);

  return G_SOURCE_REMOVE;
}

static void
prefs_dialog_save_web_application (PrefsDialog *dialog)
{
  if (!dialog->webapp)
    return;

  if (dialog->webapp_save_id)
    g_source_remove (dialog->webapp_save_id);

  dialog->webapp_save_id = g_timeout_add_seconds (1, (GSourceFunc)save_web_application, dialog);
}

static void
prefs_dialog_update_webapp_icon (PrefsDialog *dialog,
                                 const char  *icon_url)
{
  GdkPixbuf *icon, *scaled_icon;
  int icon_width, icon_height;
  double scale;

  icon = gdk_pixbuf_new_from_file (icon_url, NULL);
  if (!icon)
    return;

  icon_width = gdk_pixbuf_get_width (icon);
  icon_height = gdk_pixbuf_get_height (icon);
  scale = MIN ((double)64 / icon_width, (double)64 / icon_height);
  scaled_icon = gdk_pixbuf_scale_simple (icon, icon_width * scale, icon_height * scale, GDK_INTERP_NEAREST);
  g_object_unref (icon);
  gtk_image_set_from_pixbuf (GTK_IMAGE (dialog->webapp_icon), scaled_icon);
  g_object_set_data_full (G_OBJECT (dialog->webapp_icon), "ephy-webapp-icon-url",
                          g_strdup (icon_url), g_free);
  g_object_unref (scaled_icon);
}

static void
webapp_icon_chooser_response_cb (GtkNativeDialog *file_chooser,
                                 int              response,
                                 PrefsDialog     *dialog)
{
  if (response == GTK_RESPONSE_ACCEPT) {
    char *icon_url;

    icon_url = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_chooser));
    prefs_dialog_update_webapp_icon (dialog, icon_url);
    g_free (icon_url);
    prefs_dialog_save_web_application (dialog);
  }

  g_object_unref (file_chooser);
}

static void
on_webapp_icon_button_clicked (GtkWidget   *button,
                               PrefsDialog *dialog)
{
  GtkFileChooser *file_chooser;
  GSList *pixbuf_formats, *l;
  GtkFileFilter *images_filter;

  file_chooser = ephy_create_file_chooser (_("Web Application Icon"),
                                           GTK_WIDGET (dialog),
                                           GTK_FILE_CHOOSER_ACTION_OPEN,
                                           EPHY_FILE_FILTER_NONE);
  images_filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (images_filter, _("Supported Image Files"));
  gtk_file_chooser_add_filter (file_chooser, images_filter);

  pixbuf_formats = gdk_pixbuf_get_formats ();
  for (l = pixbuf_formats; l; l = g_slist_next (l)) {
    GdkPixbufFormat *format = (GdkPixbufFormat *)l->data;
    GtkFileFilter *filter;
    gchar *name;
    gchar **mime_types;
    guint i;

    if (gdk_pixbuf_format_is_disabled (format) || !gdk_pixbuf_format_is_writable (format))
      continue;

    filter = gtk_file_filter_new ();
    name = gdk_pixbuf_format_get_description (format);
    gtk_file_filter_set_name (filter, name);
    g_free (name);

    mime_types = gdk_pixbuf_format_get_mime_types (format);
    for (i = 0; mime_types[i] != 0; i++) {
      gtk_file_filter_add_mime_type (images_filter, mime_types[i]);
      gtk_file_filter_add_mime_type (filter, mime_types[i]);
    }
    g_strfreev (mime_types);

    gtk_file_chooser_add_filter (file_chooser, filter);
  }
  g_slist_free (pixbuf_formats);

  g_signal_connect (file_chooser, "response",
                    G_CALLBACK (webapp_icon_chooser_response_cb),
                    dialog);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (file_chooser));
}

static void
on_webapp_entry_changed (GtkEditable *editable,
                         PrefsDialog *dialog)
{
  prefs_dialog_save_web_application (dialog);
}

static void
on_manage_webapp_additional_urls_button_clicked (GtkWidget   *button,
                                                 PrefsDialog *dialog)
{
  EphyWebappAdditionalURLsDialog *urls_dialog;

  urls_dialog = ephy_webapp_additional_urls_dialog_new ();
  gtk_window_set_transient_for (GTK_WINDOW (urls_dialog), GTK_WINDOW (dialog));
  gtk_window_set_modal (GTK_WINDOW (urls_dialog), TRUE);
  gtk_window_present (GTK_WINDOW (urls_dialog));
}

static void
on_manage_cookies_button_clicked (GtkWidget   *button,
                                  PrefsDialog *dialog)
{
  EphyCookiesDialog *cookies_dialog;

  cookies_dialog = ephy_cookies_dialog_new ();

  gtk_window_set_transient_for (GTK_WINDOW (cookies_dialog), GTK_WINDOW (dialog));
  gtk_window_set_modal (GTK_WINDOW (cookies_dialog), TRUE);
  gtk_window_present (GTK_WINDOW (cookies_dialog));
}

static void
on_manage_passwords_button_clicked (GtkWidget   *button,
                                    PrefsDialog *dialog)
{
  EphyPasswordsDialog *passwords_dialog;
  EphyPasswordManager *password_manager;

  password_manager = ephy_embed_shell_get_password_manager (EPHY_EMBED_SHELL(ephy_shell_get_default ()));
  passwords_dialog = ephy_passwords_dialog_new (password_manager);

  gtk_window_set_transient_for (GTK_WINDOW (passwords_dialog), GTK_WINDOW (dialog));
  gtk_window_set_modal (GTK_WINDOW (passwords_dialog), TRUE);
  gtk_window_present (GTK_WINDOW (passwords_dialog));
}

static void
on_search_engine_dialog_button_clicked (GtkWidget   *button,
                                        PrefsDialog *dialog)
{
  GtkWindow *search_engine_dialog;

  search_engine_dialog = GTK_WINDOW (ephy_search_engine_dialog_new ());

  gtk_window_set_transient_for (search_engine_dialog, GTK_WINDOW (dialog));
  gtk_window_set_modal (search_engine_dialog, TRUE);
  gtk_window_present (search_engine_dialog);
}

static gboolean
on_default_zoom_spin_button_output (GtkSpinButton *spin,
                                    gpointer       user_data)
{
  GtkAdjustment *adjustment;
  g_autofree gchar *text = NULL;
  gdouble value;

  adjustment = gtk_spin_button_get_adjustment (spin);
  value = (int)gtk_adjustment_get_value (adjustment);
  text = g_strdup_printf ("%.f%%", value);
  gtk_entry_set_text (GTK_ENTRY (spin), text);

  return TRUE;
}

static void
on_default_zoom_spin_button_value_changed (GtkSpinButton *spin,
                                           gpointer       user_data)
{
  GtkAdjustment *adjustment;
  gdouble value;

  adjustment = gtk_spin_button_get_adjustment (spin);
  value = gtk_adjustment_get_value (adjustment);
  value = roundf(value) / 100;
  g_settings_set_double (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_DEFAULT_ZOOM_LEVEL, value);
}

static void
prefs_dialog_class_init (PrefsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = prefs_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/prefs-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, notebook);

  /* general */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, homepage_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, homepage_list);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, new_tab_homepage_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, blank_homepage_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, custom_homepage_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, custom_homepage_entry);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, webapp_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, webapp_list);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, webapp_icon);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, webapp_url);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, webapp_title);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, search_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, search_engines_list);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, session_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, browsing_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, browsing_list);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, session_list);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, restore_session_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, popups_allow_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, web_content_list);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, adblock_allow_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, enable_safe_browsing_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, enable_smooth_scrolling_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, downloads_list);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, download_folder_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, download_box);

  /* fonts & style */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, fonts_list);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, use_gnome_fonts_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, use_custom_fonts_list);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sans_fontbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, serif_fontbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, mono_fontbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, css_list);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, css_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, css_edit_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, default_zoom_spin_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, reader_mode_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, reader_mode_list);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, reader_mode_font_style);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, reader_mode_color_scheme);

  /* stored data */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, cookies_list);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, always);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, no_third_party);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, never);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, passwords_list);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, remember_passwords_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, personal_data_list);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, do_not_track_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, do_not_track_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, clear_personal_data_button);

  /* language */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, lang_treeview);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, lang_add_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, lang_remove_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, lang_up_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, lang_down_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, spell_checking_list);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, enable_spell_checking_switch);

  /* sync */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_page_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_firefox_iframe_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_firefox_iframe_label);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_firefox_account_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_firefox_account_label);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_sign_out_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_options_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_bookmarks_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_passwords_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_history_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_open_tabs_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_frequency_5_min_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_frequency_15_min_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_frequency_30_min_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_frequency_60_min_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_now_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, synced_tabs_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_device_name_entry);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_device_name_change_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_device_name_save_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_device_name_cancel_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_last_sync_time_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_last_sync_time_label);

  gtk_widget_class_bind_template_callback (widget_class, on_webapp_icon_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_webapp_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_manage_webapp_additional_urls_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_manage_cookies_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_manage_passwords_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_search_engine_dialog_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_sync_sign_out_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_sync_sync_now_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_sync_synced_tabs_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_sync_device_name_change_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_sync_device_name_save_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_sync_device_name_cancel_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_default_zoom_spin_button_output);
  gtk_widget_class_bind_template_callback (widget_class, on_default_zoom_spin_button_value_changed);
}

static void
css_file_opened_cb (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  gboolean ret;
  GError *error = NULL;

  ret = ephy_open_file_via_flatpak_portal_finish (result, &error);
  if (!ret) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Failed to open CSS file: %s", error->message);
    g_error_free (error);
  }
}

static void
css_file_created_cb (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  GFile *file = G_FILE (source);
  GFileOutputStream *stream;
  GError *error = NULL;

  stream = g_file_create_finish (file, result, &error);
  if (stream == NULL && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    g_warning ("Failed to create %s: %s", g_file_get_path (file), error->message);
  else
    ephy_open_file_via_flatpak_portal (g_file_get_path (file), NULL, css_file_opened_cb, NULL);

  if (error != NULL)
    g_error_free (error);
  if (stream != NULL)
    g_object_unref (stream);
  g_object_unref (file);
}

static void
css_edit_button_clicked_cb (GtkWidget   *button,
                            PrefsDialog *pd)
{
  GFile *css_file;

  css_file = g_file_new_for_path (g_build_filename (ephy_dot_dir (),
                                                    USER_STYLESHEET_FILENAME,
                                                    NULL));

  if (ephy_is_running_inside_flatpak ()) {
    g_file_create_async (css_file, G_FILE_CREATE_NONE, G_PRIORITY_DEFAULT, NULL, css_file_created_cb, NULL);
  } else {
    ephy_file_launch_handler (css_file, gtk_get_current_event_time ());
    g_object_unref (css_file);
  }
}

static void
language_editor_add (PrefsDialog *pd,
                     const char  *code,
                     const char  *desc)
{
  GtkTreeIter iter;

  g_assert (code != NULL && desc != NULL);

  if (gtk_tree_model_get_iter_first (pd->lang_model, &iter)) {
    do {
      char *c;

      gtk_tree_model_get (pd->lang_model, &iter,
                          COL_LANG_CODE, &c,
                          -1);

      if (strcmp (code, c) == 0) {
        g_free (c);

        /* already in list, don't allow a duplicate */
        return;
      }
      g_free (c);
    } while (gtk_tree_model_iter_next (pd->lang_model, &iter));
  }

  gtk_list_store_append (GTK_LIST_STORE (pd->lang_model), &iter);

  gtk_list_store_set (GTK_LIST_STORE (pd->lang_model), &iter,
                      COL_LANG_NAME, desc,
                      COL_LANG_CODE, code,
                      -1);
}

static void
language_editor_update_pref (PrefsDialog *pd)
{
  GtkTreeIter iter;
  GVariantBuilder builder;

  if (gtk_tree_model_get_iter_first (pd->lang_model, &iter)) {
    g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);

    do {
      char *code;

      gtk_tree_model_get (pd->lang_model, &iter,
                          COL_LANG_CODE, &code,
                          -1);
      g_variant_builder_add (&builder, "s", code);
      g_free (code);
    } while (gtk_tree_model_iter_next (pd->lang_model, &iter));

    g_settings_set (EPHY_SETTINGS_WEB,
                    EPHY_PREFS_WEB_LANGUAGE,
                    "as", &builder);
  } else {
    g_settings_set (EPHY_SETTINGS_WEB,
                    EPHY_PREFS_WEB_LANGUAGE,
                    "as", NULL);
  }
}

static void
language_editor_update_buttons (PrefsDialog *dialog)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  gboolean can_remove = FALSE, can_move_up = FALSE, can_move_down = FALSE;
  int selected;

  selection = gtk_tree_view_get_selection (dialog->lang_treeview);

  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    path = gtk_tree_model_get_path (model, &iter);

    selected = gtk_tree_path_get_indices (path)[0];

    can_remove = TRUE;
    can_move_up = selected > 0;
    can_move_down =
      selected < gtk_tree_model_iter_n_children (model, NULL) - 1;

    gtk_tree_path_free (path);
  }

  gtk_widget_set_sensitive (dialog->lang_remove_button, can_remove);
  gtk_widget_set_sensitive (dialog->lang_up_button, can_move_up);
  gtk_widget_set_sensitive (dialog->lang_down_button, can_move_down);
}

static void
add_lang_dialog_selection_changed (GtkTreeSelection *selection,
                                   GtkWidget        *button)
{
  int n_selected;

  n_selected = gtk_tree_selection_count_selected_rows (selection);
  gtk_widget_set_sensitive (button, n_selected > 0);
}

static void
add_lang_dialog_response_cb (GtkWidget   *widget,
                             int          response,
                             PrefsDialog *pd)
{
  GtkDialog *dialog = pd->add_lang_dialog;
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GList *rows, *r;

  g_assert (dialog != NULL);

  if (response == GTK_RESPONSE_ACCEPT) {
    selection = gtk_tree_view_get_selection (pd->add_lang_treeview);

    rows = gtk_tree_selection_get_selected_rows (selection, &model);

    for (r = rows; r != NULL; r = r->next) {
      GtkTreePath *path = (GtkTreePath *)r->data;

      if (gtk_tree_model_get_iter (model, &iter, path)) {
        char *code, *desc;

        gtk_tree_model_get (model, &iter,
                            COL_LANG_NAME, &desc,
                            COL_LANG_CODE, &code,
                            -1);

        language_editor_add (pd, code, desc);

        g_free (desc);
        g_free (code);
      }
    }

    g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
    g_list_free (rows);

    language_editor_update_pref (pd);
    language_editor_update_buttons (pd);
  }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
add_system_language_entry (GtkListStore *store)
{
  GtkTreeIter iter;
  char **sys_langs;
  char *system, *text;
  int n_sys_langs;

  sys_langs = ephy_langs_get_languages ();
  n_sys_langs = g_strv_length (sys_langs);

  system = g_strjoinv (", ", sys_langs);

  text = g_strdup_printf
           (ngettext ("System language (%s)",
                      "System languages (%s)", n_sys_langs), system);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
                      COL_LANG_NAME, text,
                      COL_LANG_CODE, "system",
                      -1);

  g_strfreev (sys_langs);
  g_free (system);
  g_free (text);
}

static GtkDialog *
setup_add_language_dialog (PrefsDialog *dialog)
{
  GtkWidget *ad;
  GtkWidget *add_button;
  GtkListStore *store;
  GtkTreeModel *sortmodel;
  GtkTreeView *treeview;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  guint i, n;
  GtkBuilder *builder;
  g_auto(GStrv) locales;

  builder = gtk_builder_new_from_resource ("/org/gnome/epiphany/gtk/prefs-lang-dialog.ui");
  ad = GTK_WIDGET (gtk_builder_get_object (builder, "add_language_dialog"));
  add_button = GTK_WIDGET (gtk_builder_get_object (builder, "add_button"));
  treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder, "languages_treeview"));
  dialog->add_lang_treeview = treeview;

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

  locales = gnome_get_all_locales ();
  n = g_strv_length (locales);

  for (i = 0; i < n; i++) {
    const char *locale = locales[i];
    g_autofree char *language_code = NULL;
    g_autofree char *country_code = NULL;
    g_autofree char *language_name = NULL;
    g_autofree char *shortened_locale = NULL;

    if (!gnome_parse_locale (locale, &language_code, &country_code, NULL, NULL))
      break;

    if (language_code == NULL)
      break;

    language_name = gnome_get_language_from_locale (locale, locale);

    if (country_code != NULL)
      shortened_locale = g_strdup_printf ("%s-%s", language_code, country_code);
    else
      shortened_locale = g_strdup (language_code);

    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
                        COL_LANG_NAME, language_name,
                        COL_LANG_CODE, shortened_locale,
                        -1);
  }

  add_system_language_entry (store);

  sortmodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_sortable_set_sort_column_id
    (GTK_TREE_SORTABLE (sortmodel), COL_LANG_NAME, GTK_SORT_ASCENDING);

  gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (dialog)),
                               GTK_WINDOW (ad));
  gtk_window_set_modal (GTK_WINDOW (ad), TRUE);

  gtk_tree_view_set_reorderable (GTK_TREE_VIEW (treeview), FALSE);

  gtk_tree_view_set_model (treeview, sortmodel);

  gtk_tree_view_set_headers_visible (treeview, FALSE);

  renderer = gtk_cell_renderer_text_new ();

  gtk_tree_view_insert_column_with_attributes (treeview,
                                               0, "Language",
                                               renderer,
                                               "text", 0,
                                               NULL);
  column = gtk_tree_view_get_column (treeview, 0);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_column_set_sort_column_id (column, COL_LANG_NAME);

  selection = gtk_tree_view_get_selection (treeview);
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

  add_lang_dialog_selection_changed (GTK_TREE_SELECTION (selection), add_button);
  g_signal_connect (selection, "changed",
                    G_CALLBACK (add_lang_dialog_selection_changed), add_button);

  g_signal_connect (ad, "response",
                    G_CALLBACK (add_lang_dialog_response_cb), dialog);

  g_object_unref (store);
  g_object_unref (sortmodel);

  return GTK_DIALOG (ad);
}

static void
language_editor_add_button_clicked_cb (GtkWidget   *button,
                                       PrefsDialog *pd)
{
  if (pd->add_lang_dialog == NULL) {
    GtkDialog **add_lang_dialog;

    pd->add_lang_dialog = setup_add_language_dialog (pd);
    gtk_window_set_transient_for (GTK_WINDOW (pd->add_lang_dialog), GTK_WINDOW (pd));

    add_lang_dialog = &pd->add_lang_dialog;

    g_object_add_weak_pointer
      (G_OBJECT (pd->add_lang_dialog),
      (gpointer *)add_lang_dialog);
  }

  gtk_window_present (GTK_WINDOW (pd->add_lang_dialog));
}

static void
language_editor_remove_button_clicked_cb (GtkWidget   *button,
                                          PrefsDialog *pd)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;

  selection = gtk_tree_view_get_selection (pd->lang_treeview);

  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
  }

  language_editor_update_pref (pd);
  language_editor_update_buttons (pd);
}

static void
language_editor_up_button_clicked_cb (GtkWidget   *button,
                                      PrefsDialog *pd)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter, iter_prev;
  GtkTreePath *path;

  selection = gtk_tree_view_get_selection (pd->lang_treeview);

  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    path = gtk_tree_model_get_path (model, &iter);

    if (!gtk_tree_path_prev (path)) {
      gtk_tree_path_free (path);
      return;
    }

    gtk_tree_model_get_iter (model, &iter_prev, path);

    gtk_list_store_swap (GTK_LIST_STORE (model), &iter_prev, &iter);

    gtk_tree_path_free (path);
  }

  language_editor_update_pref (pd);
  language_editor_update_buttons (pd);
}

static void
language_editor_down_button_clicked_cb (GtkWidget   *button,
                                        PrefsDialog *pd)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter, iter_next;
  GtkTreePath *path;


  selection = gtk_tree_view_get_selection (pd->lang_treeview);

  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    path = gtk_tree_model_get_path (model, &iter);

    gtk_tree_path_next (path);

    gtk_tree_model_get_iter (model, &iter_next, path);

    gtk_list_store_swap (GTK_LIST_STORE (model), &iter, &iter_next);

    gtk_tree_path_free (path);
  }

  language_editor_update_pref (pd);
  language_editor_update_buttons (pd);
}

static void
language_editor_treeview_drag_end_cb (GtkWidget      *widget,
                                      GdkDragContext *context,
                                      PrefsDialog    *dialog)
{
  language_editor_update_pref (dialog);
  language_editor_update_buttons (dialog);
}

static void
language_editor_selection_changed_cb (GtkTreeSelection *selection,
                                      PrefsDialog      *dialog)
{
  language_editor_update_buttons (dialog);
}

static char *
normalize_locale (const char *locale)
{
  char *result = g_strdup (locale);

  /* The result we store in prefs looks like es-ES or en-US. We don't
   * store codeset (not used in Accept-Langs) and we store with hyphen
   * instead of underscore (ditto). So here we just uppercase the
   * country code, converting e.g. es-es to es-ES. We have to do this
   * because older versions of Epiphany stored locales as entirely
   * lowercase.
   */
  for (char *p = strchr (result, '-'); p != NULL && *p != '\0'; p++)
    *p = g_ascii_toupper (*p);

  return result;
}

static char *
language_for_locale (const char *locale)
{
  g_autoptr(GString) string = g_string_new (locale);

  /* Before calling gnome_get_language_from_locale() we have to convert
   * from web locales (e.g. es-ES) to UNIX (e.g. es_ES.UTF-8).
   */
  g_strdelimit (string->str, "-", '_');
  g_string_append (string, ".UTF-8");

  return gnome_get_language_from_locale (string->str, string->str);
}

static void
create_language_section (PrefsDialog *dialog)
{
  GtkListStore *store;
  GtkTreeView *treeview;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkTreeSelection *selection;
  char **list = NULL;
  int i;

  g_signal_connect (dialog->lang_add_button, "clicked",
                    G_CALLBACK (language_editor_add_button_clicked_cb), dialog);
  g_signal_connect (dialog->lang_remove_button, "clicked",
                    G_CALLBACK (language_editor_remove_button_clicked_cb), dialog);
  g_signal_connect (dialog->lang_up_button, "clicked",
                    G_CALLBACK (language_editor_up_button_clicked_cb), dialog);
  g_signal_connect (dialog->lang_down_button, "clicked",
                    G_CALLBACK (language_editor_down_button_clicked_cb), dialog);

  /* setup the languages treeview */
  treeview = dialog->lang_treeview;

  gtk_tree_view_set_reorderable (treeview, TRUE);
  gtk_tree_view_set_headers_visible (treeview, FALSE);

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

  dialog->lang_model = GTK_TREE_MODEL (store);
  gtk_tree_view_set_model (treeview, dialog->lang_model);

  renderer = gtk_cell_renderer_text_new ();

  gtk_tree_view_insert_column_with_attributes (treeview,
                                               0, _("Language"),
                                               renderer,
                                               "text", 0,
                                               NULL);
  column = gtk_tree_view_get_column (treeview, 0);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_column_set_sort_column_id (column, COL_LANG_NAME);

  selection = gtk_tree_view_get_selection (treeview);
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  /* Connect treeview signals */
  g_signal_connect (G_OBJECT (treeview), "drag_end",
                    G_CALLBACK (language_editor_treeview_drag_end_cb), dialog);
  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (language_editor_selection_changed_cb), dialog);

  list = g_settings_get_strv (EPHY_SETTINGS_WEB,
                              EPHY_PREFS_WEB_LANGUAGE);

  /* Fill languages editor */
  for (i = 0; list[i]; i++) {
    const char *code = list[i];
    if (strcmp (code, "system") == 0) {
      add_system_language_entry (store);
    } else if (code[0] != '\0') {
      g_autofree char *normalized_locale = normalize_locale (code);
      if (normalized_locale != NULL) {
        g_autofree char *language_name = language_for_locale (normalized_locale);
        language_editor_add (dialog, normalized_locale, language_name);
      }
    }
  }
  g_object_unref (store);

  language_editor_update_buttons (dialog);
  g_strfreev (list);

  /* Lockdown if key is not writable */
  g_settings_bind_writable (EPHY_SETTINGS_WEB,
                            EPHY_PREFS_WEB_LANGUAGE,
                            dialog->lang_add_button, "sensitive", FALSE);
  g_settings_bind_writable (EPHY_SETTINGS_WEB,
                            EPHY_PREFS_WEB_LANGUAGE,
                            dialog->lang_remove_button, "sensitive", FALSE);
  g_settings_bind_writable (EPHY_SETTINGS_WEB,
                            EPHY_PREFS_WEB_LANGUAGE,
                            dialog->lang_up_button, "sensitive", FALSE);
  g_settings_bind_writable (EPHY_SETTINGS_WEB,
                            EPHY_PREFS_WEB_LANGUAGE,
                            dialog->lang_down_button, "sensitive", FALSE);
  g_settings_bind_writable (EPHY_SETTINGS_WEB,
                            EPHY_PREFS_WEB_LANGUAGE,
                            dialog->lang_treeview, "sensitive", FALSE);
}

static void
download_path_changed_cb (GtkFileChooser *button)
{
  char *dir;

  dir = gtk_file_chooser_get_filename (button);
  if (dir == NULL) return;

  g_settings_set_string (EPHY_SETTINGS_STATE,
                         EPHY_PREFS_STATE_DOWNLOAD_DIR, dir);
  g_free (dir);
}

static void
create_download_path_button (PrefsDialog *dialog)
{
  GtkWidget *button;
  char *dir;

  dir = ephy_file_get_downloads_dir ();

  button = gtk_file_chooser_button_new (_("Select a directory"),
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);

  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (button), dir);
  gtk_file_chooser_button_set_width_chars (GTK_FILE_CHOOSER_BUTTON (button),
                                           DOWNLOAD_BUTTON_WIDTH);
  g_signal_connect (button, "selection-changed",
                    G_CALLBACK (download_path_changed_cb), dialog);
  hdy_action_row_add_action (HDY_ACTION_ROW (dialog->download_folder_row), button);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_widget_show (button);

  g_settings_bind_writable (EPHY_SETTINGS_STATE,
                            EPHY_PREFS_STATE_DOWNLOAD_DIR,
                            button, "sensitive", FALSE);
  g_free (dir);
}

static void
prefs_dialog_response_cb (GtkWidget   *widget,
                          int          response,
                          PrefsDialog *dialog)
{
  if (dialog->webapp_save_id) {
    g_source_remove (dialog->webapp_save_id);
    save_web_application (dialog);
  }

  gtk_widget_destroy (widget);
}

static void
do_not_track_switch_activated_cb (GtkWidget   *button,
                                  PrefsDialog *dialog)
{
  char **filters;
  char **new_filters;

  filters = g_settings_get_strv (EPHY_SETTINGS_MAIN, EPHY_PREFS_ADBLOCK_FILTERS);
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    new_filters = ephy_strv_append ((const char * const *)filters, ADBLOCK_PRIVACY_FILTER_URL);
  else
    new_filters = ephy_strv_remove ((const char * const *)filters, ADBLOCK_PRIVACY_FILTER_URL);
  g_settings_set_strv (EPHY_SETTINGS_MAIN, EPHY_PREFS_ADBLOCK_FILTERS, (const char * const *)new_filters);

  g_strfreev (filters);
  g_strfreev (new_filters);
}

static void
clear_personal_data_button_clicked_cb (GtkWidget   *button,
                                       PrefsDialog *dialog)
{
  ClearDataDialog *clear_dialog;

  clear_dialog = g_object_new (EPHY_TYPE_CLEAR_DATA_DIALOG,
                               "use-header-bar", TRUE,
                               NULL);
  gtk_window_set_transient_for (GTK_WINDOW (clear_dialog), GTK_WINDOW (dialog));
  gtk_window_set_modal (GTK_WINDOW (clear_dialog), TRUE);
  gtk_window_present (GTK_WINDOW (clear_dialog));
}

static gboolean
sync_frequency_get_mapping (GValue   *value,
                            GVariant *variant,
                            gpointer  user_data)
{
  if (GPOINTER_TO_UINT (user_data) == g_variant_get_uint32 (variant))
    g_value_set_boolean (value, TRUE);

  return TRUE;
}

static GVariant *
sync_frequency_set_mapping (const GValue       *value,
                            const GVariantType *expected_type,
                            gpointer            user_data)
{
  if (!g_value_get_boolean (value))
    return NULL;

  return g_variant_new_uint32 (GPOINTER_TO_UINT (user_data));
}

static gboolean
cookies_get_mapping (GValue   *value,
                     GVariant *variant,
                     gpointer  user_data)
{
  const char *setting;
  const char *name;

  setting = g_variant_get_string (variant, NULL);
  name = gtk_buildable_get_name (GTK_BUILDABLE (user_data));

  if (g_strcmp0 (name, "no_third_party") == 0)
    name = "no-third-party";

  /* If the button name matches the setting, it should be active. */
  if (g_strcmp0 (name, setting) == 0)
    g_value_set_boolean (value, TRUE);

  return TRUE;
}

static GVariant *
cookies_set_mapping (const GValue       *value,
                     const GVariantType *expected_type,
                     gpointer            user_data)
{
  GVariant *variant = NULL;
  const char *name;

  /* Don't act unless the button has been activated (turned ON). */
  if (!g_value_get_boolean (value))
    return NULL;

  name = gtk_buildable_get_name (GTK_BUILDABLE (user_data));
  if (g_strcmp0 (name, "no_third_party") == 0)
    variant = g_variant_new_string ("no-third-party");
  else
    variant = g_variant_new_string (name);

  return variant;
}

static gboolean
new_tab_homepage_get_mapping (GValue   *value,
                              GVariant *variant,
                              gpointer  user_data)
{
  const char *setting;

  setting = g_variant_get_string (variant, NULL);
  if (!setting || setting[0] == '\0')
    g_value_set_boolean (value, TRUE);

  return TRUE;
}

static GVariant *
new_tab_homepage_set_mapping (const GValue       *value,
                              const GVariantType *expected_type,
                              gpointer            user_data)
{
  PrefsDialog *dialog = EPHY_PREFS_DIALOG (user_data);

  if (!g_value_get_boolean (value))
    return NULL;

  /* In case the new tab button is pressed while there's text in the custom homepage entry */
  gtk_entry_set_text (GTK_ENTRY (dialog->custom_homepage_entry), "");
  gtk_widget_set_sensitive (dialog->custom_homepage_entry, FALSE);

  return g_variant_new_string ("");
}

static gboolean
blank_homepage_get_mapping (GValue   *value,
                            GVariant *variant,
                            gpointer  user_data)
{
  const char *setting;

  setting = g_variant_get_string (variant, NULL);
  if (g_strcmp0 (setting, "about:blank") == 0)
      g_value_set_boolean (value, TRUE);

  return TRUE;
}

static GVariant *
blank_homepage_set_mapping (const GValue       *value,
                            const GVariantType *expected_type,
                            gpointer            user_data)
{
  PrefsDialog *dialog = EPHY_PREFS_DIALOG (user_data);

  if (!g_value_get_boolean (value))
    return NULL;

  gtk_entry_set_text (GTK_ENTRY (dialog->custom_homepage_entry), "");

  return g_variant_new_string ("about:blank");
}

static gboolean
custom_homepage_get_mapping (GValue   *value,
                             GVariant *variant,
                             gpointer  user_data)
{
  const char *setting;

  setting = g_variant_get_string (variant, NULL);
  if (setting && setting[0] != '\0' && g_strcmp0 (setting, "about:blank") != 0)
    g_value_set_boolean (value, TRUE);
  return TRUE;
}

static GVariant *
custom_homepage_set_mapping (const GValue       *value,
                             const GVariantType *expected_type,
                             gpointer            user_data)
{
  PrefsDialog *dialog = EPHY_PREFS_DIALOG (user_data);
  const char *setting;

  if (!g_value_get_boolean (value)) {
    gtk_widget_set_sensitive (dialog->custom_homepage_entry, FALSE);
    gtk_entry_set_text (GTK_ENTRY (dialog->custom_homepage_entry), "");
    return NULL;
  }

  gtk_widget_set_sensitive (dialog->custom_homepage_entry, TRUE);
  gtk_widget_grab_focus (dialog->custom_homepage_entry);
  setting = gtk_entry_get_text (GTK_ENTRY (dialog->custom_homepage_entry));
  if (!setting || setting[0] == '\0')
    return NULL;

  gtk_entry_set_text (GTK_ENTRY (dialog->custom_homepage_entry), setting);

  return g_variant_new_string (setting);
}

static void
custom_homepage_entry_changed (GtkEntry    *entry,
                               PrefsDialog *dialog)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->custom_homepage_radiobutton))) {
    g_settings_set_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_HOMEPAGE_URL,
                           gtk_entry_get_text (entry));
  } else if ((gtk_entry_get_text (entry) != NULL) &&
             gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->new_tab_homepage_radiobutton))) {
    g_settings_set_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_HOMEPAGE_URL, gtk_entry_get_text (entry));
    gtk_widget_set_sensitive (dialog->custom_homepage_entry, TRUE);
    gtk_widget_grab_focus (dialog->custom_homepage_entry);
  }
}

static void
custom_homepage_entry_icon_released (GtkEntry            *entry,
                                     GtkEntryIconPosition icon_pos)
{
  if (icon_pos == GTK_ENTRY_ICON_SECONDARY)
    gtk_entry_set_text (entry, "");
}

static gboolean
restore_session_get_mapping (GValue   *value,
                             GVariant *variant,
                             gpointer  user_data)
{
  const char *policy = g_variant_get_string (variant, NULL);
  /* FIXME: Is it possible to somehow use EPHY_PREFS_RESTORE_SESSION_POLICY_ALWAYS here? */
  g_value_set_boolean (value, !strcmp (policy, "always"));
  return TRUE;
}

static GVariant *
restore_session_set_mapping (const GValue       *value,
                             const GVariantType *expected_type,
                             gpointer            user_data)
{
  /* FIXME: Is it possible to somehow use EphyPrefsRestoreSessionPolicy here? */
  if (g_value_get_boolean (value))
    return g_variant_new_string ("always");
  return g_variant_new_string ("crashed");
}

static void
setup_general_page (PrefsDialog *dialog)
{
  GSettings *settings;
  GSettings *web_settings;

  if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_APPLICATION) {
    dialog->webapp = ephy_web_application_for_profile_directory (ephy_dot_dir ());
    prefs_dialog_update_webapp_icon (dialog, dialog->webapp->icon_url);
    gtk_entry_set_text (GTK_ENTRY (dialog->webapp_url), dialog->webapp->url);
    gtk_entry_set_text (GTK_ENTRY (dialog->webapp_title), dialog->webapp->name);
  }

  settings = ephy_settings_get (EPHY_PREFS_SCHEMA);
  web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);

  g_settings_bind_with_mapping (settings,
                                EPHY_PREFS_HOMEPAGE_URL,
                                dialog->new_tab_homepage_radiobutton,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                new_tab_homepage_get_mapping,
                                new_tab_homepage_set_mapping,
                                dialog,
                                NULL);
  g_settings_bind_with_mapping (settings,
                                EPHY_PREFS_HOMEPAGE_URL,
                                dialog->blank_homepage_radiobutton,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                blank_homepage_get_mapping,
                                blank_homepage_set_mapping,
                                dialog,
                                NULL);
  g_settings_bind_with_mapping (settings,
                                EPHY_PREFS_HOMEPAGE_URL,
                                dialog->custom_homepage_radiobutton,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                custom_homepage_get_mapping,
                                custom_homepage_set_mapping,
                                dialog,
                                NULL);
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->custom_homepage_radiobutton))) {
    gtk_widget_set_sensitive (dialog->custom_homepage_entry, TRUE);
    gtk_entry_set_text (GTK_ENTRY (dialog->custom_homepage_entry),
                        g_settings_get_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_HOMEPAGE_URL));
  } else {
    gtk_widget_set_sensitive (dialog->custom_homepage_entry, FALSE);
    gtk_entry_set_text (GTK_ENTRY (dialog->custom_homepage_entry), "");
  }

  g_signal_connect (dialog->custom_homepage_entry, "changed",
                    G_CALLBACK (custom_homepage_entry_changed),
                    dialog);
  g_signal_connect (dialog->custom_homepage_entry, "icon-release",
                    G_CALLBACK (custom_homepage_entry_icon_released),
                    NULL);

  g_settings_bind_with_mapping (settings,
                                EPHY_PREFS_RESTORE_SESSION_POLICY,
                                dialog->restore_session_switch,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                restore_session_get_mapping,
                                restore_session_set_mapping,
                                NULL, NULL);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_POPUPS,
                   dialog->popups_allow_switch,
                   "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_SAFE_BROWSING,
                   dialog->enable_safe_browsing_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_ADBLOCK,
                   dialog->adblock_allow_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_ADBLOCK,
                   dialog->do_not_track_row,
                   "sensitive",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_DO_NOT_TRACK,
                   dialog->do_not_track_switch,
                   "active",
                   /* Teensy hack: don't override the previous binding. */
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_signal_connect (dialog->do_not_track_switch,
                    "notify::active",
                    G_CALLBACK (do_not_track_switch_activated_cb),
                    dialog);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_SMOOTH_SCROLLING,
                   dialog->enable_smooth_scrolling_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  if (ephy_is_running_inside_flatpak ())
    gtk_widget_hide (dialog->download_box);
  else
    create_download_path_button (dialog);
}

static gchar *
reader_font_style_get_name (HdyEnumValueObject *value,
                            gpointer            user_data)
{
  g_assert (HDY_IS_ENUM_VALUE_OBJECT (value));

  switch (hdy_enum_value_object_get_value (value)) {
  case EPHY_PREFS_READER_FONT_STYLE_SANS:
    return g_strdup (N_("Sans"));
  case EPHY_PREFS_READER_FONT_STYLE_SERIF:
    return g_strdup (N_("Serif"));
  default:
    return NULL;
  }
}

static gboolean
reader_font_style_get_mapping (GValue   *value,
                               GVariant *variant,
                               gpointer  user_data)
{
  const char *reader_colors = g_variant_get_string (variant, NULL);

  if (g_strcmp0 (reader_colors, "sans") == 0)
    g_value_set_int (value, EPHY_PREFS_READER_FONT_STYLE_SANS);
  else if (g_strcmp0 (reader_colors, "serif") == 0)
    g_value_set_int (value, EPHY_PREFS_READER_FONT_STYLE_SERIF);

  return TRUE;
}

static GVariant *
reader_font_style_set_mapping (const GValue       *value,
                               const GVariantType *expected_type,
                               gpointer            user_data)
{
  switch (g_value_get_int (value)) {
  case EPHY_PREFS_READER_FONT_STYLE_SANS:
    return g_variant_new_string ("sans");
  case EPHY_PREFS_READER_FONT_STYLE_SERIF:
    return g_variant_new_string ("serif");
  default:
    return g_variant_new_string ("crashed");
  }
}

static gchar *
reader_color_scheme_get_name (HdyEnumValueObject *value,
                              gpointer            user_data)
{
  g_assert (HDY_IS_ENUM_VALUE_OBJECT (value));

  switch (hdy_enum_value_object_get_value (value)) {
  case EPHY_PREFS_READER_COLORS_LIGHT:
    return g_strdup (N_("Light"));
  case EPHY_PREFS_READER_COLORS_DARK:
    return g_strdup (N_("Dark"));
  default:
    return NULL;
  }
}

static gboolean
reader_color_scheme_get_mapping (GValue   *value,
                                 GVariant *variant,
                                 gpointer  user_data)
{
  const char *reader_colors = g_variant_get_string (variant, NULL);

  if (g_strcmp0 (reader_colors, "light") == 0)
    g_value_set_int (value, EPHY_PREFS_READER_COLORS_LIGHT);
  else if (g_strcmp0 (reader_colors, "dark") == 0)
    g_value_set_int (value, EPHY_PREFS_READER_COLORS_DARK);

  return TRUE;
}

static GVariant *
reader_color_scheme_set_mapping (const GValue       *value,
                                 const GVariantType *expected_type,
                                 gpointer            user_data)
{
  switch (g_value_get_int (value)) {
  case EPHY_PREFS_READER_COLORS_LIGHT:
    return g_variant_new_string ("light");
  case EPHY_PREFS_READER_COLORS_DARK:
    return g_variant_new_string ("dark");
  default:
    return g_variant_new_string ("crashed");
  }
}

static void
setup_fonts_page (PrefsDialog *dialog)
{
  GSettings *web_settings;
  GSettings *reader_settings;

  web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);
  reader_settings = ephy_settings_get (EPHY_PREFS_READER_SCHEMA);

  hdy_combo_row_set_for_enum (HDY_COMBO_ROW (dialog->reader_mode_font_style),
                              EPHY_TYPE_PREFS_READER_FONT_STYLE,
                              reader_font_style_get_name, NULL, NULL);
  g_settings_bind_with_mapping (reader_settings,
                                EPHY_PREFS_READER_FONT_STYLE,
                                dialog->reader_mode_font_style,
                                "selected-index",
                                G_SETTINGS_BIND_DEFAULT,
                                reader_font_style_get_mapping,
                                reader_font_style_set_mapping,
                                NULL, NULL);

  hdy_combo_row_set_for_enum (HDY_COMBO_ROW (dialog->reader_mode_color_scheme),
                              EPHY_TYPE_PREFS_READER_COLOR_SCHEME,
                              reader_color_scheme_get_name, NULL, NULL);
  g_settings_bind_with_mapping (reader_settings,
                                EPHY_PREFS_READER_COLOR_SCHEME,
                                dialog->reader_mode_color_scheme,
                                "selected-index",
                                G_SETTINGS_BIND_DEFAULT,
                                reader_color_scheme_get_mapping,
                                reader_color_scheme_set_mapping,
                                NULL, NULL);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_USE_GNOME_FONTS,
                   dialog->use_gnome_fonts_row,
                   "enable-expansion",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_SANS_SERIF_FONT,
                   dialog->sans_fontbutton,
                   "font-name",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_SERIF_FONT,
                   dialog->serif_fontbutton,
                   "font-name",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_MONOSPACE_FONT,
                   dialog->mono_fontbutton,
                   "font-name",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_USER_CSS,
                   dialog->css_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_USER_CSS,
                   dialog->css_edit_button,
                   "sensitive",
                   G_SETTINGS_BIND_GET);

  g_signal_connect (dialog->css_edit_button,
                    "clicked",
                    G_CALLBACK (css_edit_button_clicked_cb),
                    dialog);

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->default_zoom_spin_button),
                             g_settings_get_double (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_DEFAULT_ZOOM_LEVEL) * 100);
}

static void
setup_stored_data_page (PrefsDialog *dialog)
{
  GSettings *web_settings;

  web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);

  g_settings_bind_with_mapping (web_settings,
                                EPHY_PREFS_WEB_COOKIES_POLICY,
                                dialog->always,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                cookies_get_mapping,
                                cookies_set_mapping,
                                dialog->always,
                                NULL);
  g_settings_bind_with_mapping (web_settings,
                                EPHY_PREFS_WEB_COOKIES_POLICY,
                                dialog->no_third_party,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                cookies_get_mapping,
                                cookies_set_mapping,
                                dialog->no_third_party,
                                NULL);
  g_settings_bind_with_mapping (web_settings,
                                EPHY_PREFS_WEB_COOKIES_POLICY,
                                dialog->never,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                cookies_get_mapping,
                                cookies_set_mapping,
                                dialog->never,
                                NULL);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_REMEMBER_PASSWORDS,
                   dialog->remember_passwords_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect (dialog->clear_personal_data_button,
                    "clicked",
                    G_CALLBACK (clear_personal_data_button_clicked_cb),
                    dialog);
}

static void
setup_language_page (PrefsDialog *dialog)
{
  GSettings *web_settings;

  web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_SPELL_CHECKING,
                   dialog->enable_spell_checking_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  create_language_section (dialog);
}

static void
setup_sync_page (PrefsDialog *dialog)
{
  EphySyncService *service = ephy_shell_get_sync_service (ephy_shell_get_default ());
  GSettings *sync_settings = ephy_settings_get (EPHY_PREFS_SYNC_SCHEMA);
  char *user = ephy_sync_utils_get_sync_user ();
  char *name = ephy_sync_utils_get_device_name ();

  gtk_entry_set_text (GTK_ENTRY (dialog->sync_device_name_entry), name);

  if (!user) {
    sync_setup_firefox_iframe (dialog);
    gtk_container_remove (GTK_CONTAINER (dialog->sync_page_box),
                          dialog->sync_firefox_account_box);
    gtk_container_remove (GTK_CONTAINER (dialog->sync_page_box),
                          dialog->sync_options_box);
  } else {
    char *email = g_strdup_printf ("<b>%s</b>", user);
    /* Translators: the %s refers to the email of the currently logged in user. */
    char *text = g_strdup_printf (_("Logged in as %s"), email);

    sync_set_last_sync_time (dialog);
    gtk_label_set_markup (GTK_LABEL (dialog->sync_firefox_account_label), text);
    gtk_container_remove (GTK_CONTAINER (dialog->sync_page_box),
                          dialog->sync_firefox_iframe_box);

    g_free (email);
    g_free (text);
  }

  g_settings_bind (sync_settings,
                   EPHY_PREFS_SYNC_BOOKMARKS_ENABLED,
                   dialog->sync_bookmarks_checkbutton,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (sync_settings,
                   EPHY_PREFS_SYNC_PASSWORDS_ENABLED,
                   dialog->sync_passwords_checkbutton,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (sync_settings,
                   EPHY_PREFS_SYNC_HISTORY_ENABLED,
                   dialog->sync_history_checkbutton,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (sync_settings,
                   EPHY_PREFS_SYNC_OPEN_TABS_ENABLED,
                   dialog->sync_open_tabs_checkbutton,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind_with_mapping (sync_settings,
                                EPHY_PREFS_SYNC_FREQUENCY,
                                dialog->sync_frequency_5_min_radiobutton,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                sync_frequency_get_mapping,
                                sync_frequency_set_mapping,
                                GINT_TO_POINTER (5),
                                NULL);
  g_settings_bind_with_mapping (sync_settings,
                                EPHY_PREFS_SYNC_FREQUENCY,
                                dialog->sync_frequency_15_min_radiobutton,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                sync_frequency_get_mapping,
                                sync_frequency_set_mapping,
                                GINT_TO_POINTER (15),
                                NULL);
  g_settings_bind_with_mapping (sync_settings,
                                EPHY_PREFS_SYNC_FREQUENCY,
                                dialog->sync_frequency_30_min_radiobutton,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                sync_frequency_get_mapping,
                                sync_frequency_set_mapping,
                                GINT_TO_POINTER (30),
                                NULL);
  g_settings_bind_with_mapping (sync_settings,
                                EPHY_PREFS_SYNC_FREQUENCY,
                                dialog->sync_frequency_60_min_radiobutton,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                sync_frequency_get_mapping,
                                sync_frequency_set_mapping,
                                GINT_TO_POINTER (60),
                                NULL);

  g_object_bind_property (dialog->sync_open_tabs_checkbutton, "active",
                          dialog->synced_tabs_button, "sensitive",
                          G_BINDING_SYNC_CREATE);

  g_signal_connect_object (service, "sync-secrets-store-finished",
                           G_CALLBACK (sync_secrets_store_finished_cb),
                           dialog, 0);
  g_signal_connect_object (service, "sync-sign-in-error",
                           G_CALLBACK (sync_sign_in_error_cb),
                           dialog, 0);
  g_signal_connect_object (service, "sync-finished",
                           G_CALLBACK (sync_finished_cb),
                           dialog, 0);
  g_signal_connect_object (dialog->sync_bookmarks_checkbutton, "toggled",
                           G_CALLBACK (sync_collection_toggled_cb),
                           dialog, 0);
  g_signal_connect_object (dialog->sync_passwords_checkbutton, "toggled",
                           G_CALLBACK (sync_collection_toggled_cb),
                           dialog, 0);
  g_signal_connect_object (dialog->sync_history_checkbutton, "toggled",
                           G_CALLBACK (sync_collection_toggled_cb),
                           dialog, 0);
  g_signal_connect_object (dialog->sync_open_tabs_checkbutton, "toggled",
                           G_CALLBACK (sync_collection_toggled_cb),
                           dialog, 0);

  g_free (user);
  g_free (name);
}

static void
prefs_dialog_init (PrefsDialog *dialog)
{
  EphyEmbedShellMode mode;

  gtk_widget_init_template (GTK_WIDGET (dialog));

  gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->browsing_list), hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->homepage_list), hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->downloads_list), hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->webapp_list), hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->search_engines_list), hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->session_list), hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->web_content_list), hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->fonts_list), hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->use_custom_fonts_list), hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->css_list), hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->reader_mode_list), hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->cookies_list), hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->passwords_list), hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->personal_data_list), hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->spell_checking_list), hdy_list_box_separator_header, NULL, NULL);

  mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ());
  gtk_widget_set_visible (dialog->webapp_box,
                          mode == EPHY_EMBED_SHELL_MODE_APPLICATION);
  gtk_widget_set_visible (dialog->homepage_box,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
  gtk_widget_set_visible (dialog->search_box,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
  gtk_widget_set_visible (dialog->session_box,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
  gtk_widget_set_visible (dialog->browsing_box,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
  gtk_widget_set_visible (dialog->do_not_track_switch,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
  gtk_widget_set_visible (dialog->enable_smooth_scrolling_switch,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
  gtk_widget_set_visible (dialog->reader_mode_box,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);

  setup_general_page (dialog);
  setup_fonts_page (dialog);
  setup_stored_data_page (dialog);
  setup_language_page (dialog);

  if (mode == EPHY_EMBED_SHELL_MODE_BROWSER)
    setup_sync_page (dialog);
  else
    gtk_notebook_remove_page (GTK_NOTEBOOK (dialog->notebook), -1);

  ephy_gui_ensure_window_group (GTK_WINDOW (dialog));
  g_signal_connect (dialog, "response",
                    G_CALLBACK (prefs_dialog_response_cb), dialog);
}
