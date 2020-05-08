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
#include "ephy-filters-manager.h"
#include "ephy-flatpak-utils.h"
#include "ephy-gui.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-prefs.h"
#include "ephy-search-engine-dialog.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-sync-service.h"
#include "ephy-sync-utils.h"
#include "ephy-time-helpers.h"
#include "clear-data-dialog.h"
#include "cookies-dialog.h"
#include "passwords-dialog.h"
#include "synced-tabs-dialog.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <JavaScriptCore/JavaScript.h>
#include <json-glib/json-glib.h>
#include <math.h>
#include <string.h>

#define FXA_IFRAME_URL "https://accounts.firefox.com/signin?service=sync&context=fx_desktop_v3"

struct _PrefsDialog {
  GtkDialog parent_instance;

  GtkWidget *notebook;

  /* Privacy */
  GtkWidget *popups_allow_switch;
  GtkWidget *adblock_allow_switch;
  GtkWidget *enable_safe_browsing_switch;

  /* fonts & style */
  GtkWidget *use_gnome_fonts_row;
  GtkWidget *use_custom_fonts_list;
  GtkWidget *sans_fontbutton;
  GtkWidget *serif_fontbutton;
  GtkWidget *mono_fontbutton;
  GtkWidget *css_switch;
  GtkWidget *css_edit_button;
  GtkWidget *default_zoom_spin_button;
  GtkWidget *reader_mode_box;
  GtkWidget *reader_mode_font_style;
  GtkWidget *reader_mode_color_scheme;

  /* stored data */
  GtkWidget *always;
  GtkWidget *no_third_party;
  GtkWidget *never;
  GtkWidget *remember_passwords_switch;
  GtkWidget *clear_personal_data_button;

  /* sync */
  GtkWidget *sync_page_box;
  GtkWidget *sync_firefox_iframe_box;
  GtkWidget *sync_firefox_iframe_label;
  GtkWidget *sync_firefox_account_box;
  GtkWidget *sync_firefox_account_row;
  GtkWidget *sync_options_box;
  GtkWidget *sync_bookmarks_checkbutton;
  GtkWidget *sync_passwords_checkbutton;
  GtkWidget *sync_history_checkbutton;
  GtkWidget *sync_open_tabs_checkbutton;
  GtkWidget *sync_frequency_row;
  GtkWidget *sync_now_button;
  GtkWidget *synced_tabs_button;
  GtkWidget *sync_device_name_entry;
  GtkWidget *sync_device_name_change_button;
  GtkWidget *sync_device_name_save_button;
  GtkWidget *sync_device_name_cancel_button;

  WebKitWebView *fxa_web_view;
  WebKitUserContentManager *fxa_manager;
  WebKitUserScript *fxa_script;
};

enum {
  COL_TITLE_ELIDED,
  COL_ENCODING,
  NUM_COLS
};

G_DEFINE_TYPE (PrefsDialog, prefs_dialog, GTK_TYPE_DIALOG)

static const guint sync_frequency_minutes[] = { 5, 15, 30, 60 };

static void
prefs_dialog_finalize (GObject *object)
{
  PrefsDialog *dialog = EPHY_PREFS_DIALOG (object);

  if (dialog->fxa_web_view != NULL) {
    webkit_user_content_manager_unregister_script_message_handler (dialog->fxa_manager,
                                                                   "toChromeMessageHandler");
    webkit_user_content_manager_unregister_script_message_handler (dialog->fxa_manager,
                                                                   "openWebmailClickHandler");
    webkit_user_script_unref (dialog->fxa_script);
    g_object_unref (dialog->fxa_manager);
  }

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

    hdy_action_row_set_subtitle (HDY_ACTION_ROW (dialog->sync_firefox_account_row), text);

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
    hdy_action_row_set_title (HDY_ACTION_ROW (dialog->sync_firefox_account_row),
                              ephy_sync_utils_get_sync_user ());
    gtk_widget_hide (dialog->sync_page_box);
    gtk_widget_show (dialog->sync_firefox_account_box);
    gtk_widget_show (dialog->sync_options_box);
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
  GtkWidget *frame;
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
    gtk_widget_set_vexpand (GTK_WIDGET (dialog->fxa_web_view), TRUE);
    gtk_widget_set_visible (GTK_WIDGET (dialog->fxa_web_view), TRUE);
    frame = gtk_frame_new (NULL);
    gtk_widget_set_visible (frame, TRUE);
    gtk_container_add (GTK_CONTAINER (frame),
                       GTK_WIDGET (dialog->fxa_web_view));
    gtk_box_pack_start (GTK_BOX (dialog->sync_firefox_iframe_box),
                        frame,
                        TRUE, TRUE, 0);

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
  gtk_widget_hide (dialog->sync_firefox_account_box);
  gtk_widget_hide (dialog->sync_options_box);
  gtk_widget_show (dialog->sync_page_box);
  hdy_action_row_set_subtitle (HDY_ACTION_ROW (dialog->sync_firefox_account_row), NULL);
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
  gtk_window_present_with_time (GTK_WINDOW (synced_tabs_dialog), gtk_get_current_event_time ());
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

static void
on_manage_cookies_button_clicked (GtkWidget   *button,
                                  PrefsDialog *dialog)
{
  EphyCookiesDialog *cookies_dialog;

  cookies_dialog = ephy_cookies_dialog_new ();

  gtk_window_set_transient_for (GTK_WINDOW (cookies_dialog), GTK_WINDOW (dialog));
  gtk_window_set_modal (GTK_WINDOW (cookies_dialog), TRUE);
  gtk_window_present_with_time (GTK_WINDOW (cookies_dialog), gtk_get_current_event_time ());
}

static void
on_manage_passwords_button_clicked (GtkWidget   *button,
                                    PrefsDialog *dialog)
{
  EphyPasswordsDialog *passwords_dialog;
  EphyPasswordManager *password_manager;

  password_manager = ephy_embed_shell_get_password_manager (EPHY_EMBED_SHELL (ephy_shell_get_default ()));
  passwords_dialog = ephy_passwords_dialog_new (password_manager);

  gtk_window_set_transient_for (GTK_WINDOW (passwords_dialog), GTK_WINDOW (dialog));
  gtk_window_set_modal (GTK_WINDOW (passwords_dialog), TRUE);
  gtk_window_present_with_time (GTK_WINDOW (passwords_dialog), gtk_get_current_event_time ());
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
  value = roundf (value) / 100;
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
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, popups_allow_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, adblock_allow_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, enable_safe_browsing_switch);

  /* fonts & style */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, use_gnome_fonts_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, use_custom_fonts_list);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sans_fontbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, serif_fontbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, mono_fontbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, css_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, css_edit_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, default_zoom_spin_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, reader_mode_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, reader_mode_font_style);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, reader_mode_color_scheme);

  /* stored data */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, always);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, no_third_party);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, never);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, remember_passwords_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, clear_personal_data_button);

  /* sync */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_page_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_firefox_iframe_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_firefox_iframe_label);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_firefox_account_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_firefox_account_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_options_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_bookmarks_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_passwords_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_history_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_open_tabs_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_frequency_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_now_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, synced_tabs_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_device_name_entry);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_device_name_change_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_device_name_save_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_device_name_cancel_button);

  gtk_widget_class_bind_template_callback (widget_class, on_manage_cookies_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_manage_passwords_button_clicked);
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
  else {
    if (ephy_is_running_inside_flatpak ())
      ephy_open_file_via_flatpak_portal (g_file_get_path (file), NULL, css_file_opened_cb, NULL);
    else
      ephy_file_launch_handler (file, gtk_get_current_event_time ());
  }

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

  css_file = g_file_new_for_path (g_build_filename (ephy_profile_dir (),
                                                    USER_STYLESHEET_FILENAME,
                                                    NULL));

  g_file_create_async (css_file, G_FILE_CREATE_NONE, G_PRIORITY_DEFAULT, NULL, css_file_created_cb, NULL);
}

static void
clear_personal_data_button_clicked_cb (GtkWidget   *button,
                                       PrefsDialog *dialog)
{
  ClearDataDialog *clear_dialog;

  clear_dialog = g_object_new (EPHY_TYPE_CLEAR_DATA_DIALOG, NULL);
  gtk_window_set_transient_for (GTK_WINDOW (clear_dialog), GTK_WINDOW (dialog));
  gtk_window_set_modal (GTK_WINDOW (clear_dialog), TRUE);
  gtk_window_present_with_time (GTK_WINDOW (clear_dialog), gtk_get_current_event_time ());
}

static gboolean
sync_frequency_get_mapping (GValue   *value,
                            GVariant *variant,
                            gpointer  user_data)
{
  uint minutes = g_variant_get_uint32 (variant);

  for (gint i = 0; i < (gint)G_N_ELEMENTS (sync_frequency_minutes); i++) {
    if (sync_frequency_minutes[i] != minutes)
      continue;

    g_value_set_int (value, i);

    return TRUE;
  }

  return FALSE;
}

static GVariant *
sync_frequency_set_mapping (const GValue       *value,
                            const GVariantType *expected_type,
                            gpointer            user_data)
{
  gint i = g_value_get_int (value);

  if (i >= (gint)G_N_ELEMENTS (sync_frequency_minutes))
    return NULL;

  return g_variant_new_uint32 (sync_frequency_minutes[i]);
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

static void
setup_general_page (PrefsDialog *dialog)
{
  GSettings *web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);

  /* TODO: Note these are actually not in the General Page but in the Privacy Page
   * Perhaps someone moved them and forgot about the function name */
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
}

static gchar *
reader_font_style_get_name (HdyEnumValueObject *value,
                            gpointer            user_data)
{
  g_assert (HDY_IS_ENUM_VALUE_OBJECT (value));

  switch (hdy_enum_value_object_get_value (value)) {
    case EPHY_PREFS_READER_FONT_STYLE_SANS:
      return g_strdup (_("Sans"));
    case EPHY_PREFS_READER_FONT_STYLE_SERIF:
      return g_strdup (_("Serif"));
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
      return g_strdup (_("Light"));
    case EPHY_PREFS_READER_COLORS_DARK:
      return g_strdup (_("Dark"));
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

static GListModel *
create_sync_frequency_minutes_model ()
{
  GListStore *list_store = g_list_store_new (HDY_TYPE_VALUE_OBJECT);
  HdyValueObject *obj;
  g_auto (GValue) value = G_VALUE_INIT;
  guint i;

  g_value_init (&value, G_TYPE_UINT);

  for (i = 0; i < G_N_ELEMENTS (sync_frequency_minutes); i++) {
    g_value_set_uint (&value, sync_frequency_minutes[i]);
    obj = hdy_value_object_new (&value);
    g_list_store_insert (list_store, i, obj);
    g_clear_object (&obj);
  }

  return G_LIST_MODEL (list_store);
}

static gchar *
get_sync_frequency_minutes_name (HdyValueObject *value)
{
  return g_strdup_printf ("%u min", g_value_get_uint (hdy_value_object_get_value (value)));
}

static void
setup_sync_page (PrefsDialog *dialog)
{
  EphySyncService *service = ephy_shell_get_sync_service (ephy_shell_get_default ());
  GSettings *sync_settings = ephy_settings_get (EPHY_PREFS_SYNC_SCHEMA);
  char *user = ephy_sync_utils_get_sync_user ();
  char *name = ephy_sync_utils_get_device_name ();
  g_autoptr (GListModel) sync_frequency_minutes_model = create_sync_frequency_minutes_model ();

  gtk_entry_set_text (GTK_ENTRY (dialog->sync_device_name_entry), name);

  if (!user) {
    sync_setup_firefox_iframe (dialog);
    gtk_widget_hide (dialog->sync_firefox_account_box);
    gtk_widget_hide (dialog->sync_options_box);
  } else {
    sync_set_last_sync_time (dialog);
    hdy_action_row_set_title (HDY_ACTION_ROW (dialog->sync_firefox_account_row), user);
    gtk_widget_hide (dialog->sync_page_box);
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

  hdy_combo_row_bind_name_model (HDY_COMBO_ROW (dialog->sync_frequency_row),
                                 sync_frequency_minutes_model,
                                 (HdyComboRowGetNameFunc)get_sync_frequency_minutes_name,
                                 NULL,
                                 NULL);
  g_settings_bind_with_mapping (sync_settings,
                                EPHY_PREFS_SYNC_FREQUENCY,
                                dialog->sync_frequency_row,
                                "selected-index",
                                G_SETTINGS_BIND_DEFAULT,
                                sync_frequency_get_mapping,
                                sync_frequency_set_mapping,
                                NULL, NULL);

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

  mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ());

  gtk_widget_set_visible (dialog->reader_mode_box,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);

  gtk_window_set_icon_name (GTK_WINDOW (dialog), APPLICATION_ID);

  setup_general_page (dialog);
  setup_fonts_page (dialog);
  setup_stored_data_page (dialog);

  if (mode == EPHY_EMBED_SHELL_MODE_BROWSER)
    setup_sync_page (dialog);
  else
    gtk_notebook_remove_page (GTK_NOTEBOOK (dialog->notebook), -1);

  ephy_gui_ensure_window_group (GTK_WINDOW (dialog));
  gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->use_custom_fonts_list), hdy_list_box_separator_header, NULL, NULL);
}
