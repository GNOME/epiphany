/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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

#include "ephy-firefox-sync-dialog.h"

#include "ephy-debug.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-sync-service.h"
#include "ephy-sync-utils.h"
#include "ephy-time-helpers.h"
#include "synced-tabs-dialog.h"

#define FXA_IFRAME_URL "https://accounts.firefox.com/signin?service=sync&context=fx_desktop_v3"

#define EPHY_TYPE_SYNC_FREQUENCY (ephy_sync_frequency_get_type ())

G_DECLARE_FINAL_TYPE (EphySyncFrequency, ephy_sync_frequency, EPHY, SYNC_FREQUENCY, GObject);

struct _EphySyncFrequency {
  GObject parent_instance;
  guint frequency;
};

G_DEFINE_FINAL_TYPE (EphySyncFrequency, ephy_sync_frequency, G_TYPE_OBJECT)

static void
ephy_sync_frequency_class_init (EphySyncFrequencyClass *klass)
{
}

static void
ephy_sync_frequency_init (EphySyncFrequency *self)
{
}

static EphySyncFrequency *
ephy_sync_frequency_new (guint frequency)
{
  EphySyncFrequency *self = g_object_new (EPHY_TYPE_SYNC_FREQUENCY, NULL);

  self->frequency = frequency;

  return self;
}

struct _EphyFirefoxSyncDialog {
  AdwWindow parent_instance;

  GtkWidget *sync_page_group;
  GtkWidget *sync_firefox_iframe_box;
  GtkWidget *sync_firefox_iframe_label;
  GtkWidget *sync_firefox_account_group;
  GtkWidget *sync_firefox_account_row;
  GtkWidget *sync_options_group;
  GtkWidget *sync_bookmarks_row;
  GtkWidget *sync_passwords_row;
  GtkWidget *sync_history_row;
  GtkWidget *sync_open_tabs_row;
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

G_DEFINE_FINAL_TYPE (EphyFirefoxSyncDialog, ephy_firefox_sync_dialog, ADW_TYPE_WINDOW)

static const guint sync_frequency_minutes[] = { 5, 15, 30, 60 };

static void
sync_collection_toggled_cb (GtkWidget             *sw,
                            gboolean               sw_active,
                            EphyFirefoxSyncDialog *sync_dialog)
{
  EphySynchronizableManager *manager = NULL;
  EphyShell *shell = ephy_shell_get_default ();
  EphySyncService *service = ephy_shell_get_sync_service (shell);

  if (GTK_WIDGET (sw) == sync_dialog->sync_bookmarks_row) {
    return; /* https://gitlab.gnome.org/GNOME/epiphany/-/issues/1118#note_1731178
    manager = EPHY_SYNCHRONIZABLE_MANAGER (ephy_shell_get_bookmarks_manager (shell)); */
  } else if (GTK_WIDGET (sw) == sync_dialog->sync_passwords_row) {
    manager = EPHY_SYNCHRONIZABLE_MANAGER (ephy_embed_shell_get_password_manager (EPHY_EMBED_SHELL (shell)));
  } else if (GTK_WIDGET (sw) == sync_dialog->sync_history_row) {
    manager = EPHY_SYNCHRONIZABLE_MANAGER (ephy_shell_get_history_manager (shell));
  } else if (GTK_WIDGET (sw) == sync_dialog->sync_open_tabs_row) {
    manager = EPHY_SYNCHRONIZABLE_MANAGER (ephy_shell_get_open_tabs_manager (shell));
    ephy_open_tabs_manager_clear_cache (EPHY_OPEN_TABS_MANAGER (manager));
  } else {
    g_assert_not_reached ();
  }

  if (sw_active) {
    ephy_sync_service_register_manager (service, manager);
  } else {
    ephy_sync_service_unregister_manager (service, manager);
    ephy_synchronizable_manager_set_is_initial_sync (manager, TRUE);
  }
}

static void
sync_set_last_sync_time (EphyFirefoxSyncDialog *sync_dialog)
{
  gint64 sync_time = ephy_sync_utils_get_sync_time ();

  if (sync_time) {
    char *time = ephy_time_helpers_utf_friendly_time (sync_time);
    /* Translators: the %s refers to the time at which the last sync was made.
     * For example: Today 04:34 PM, Sun 11:25 AM, May 31 06:41 PM.
     */
    char *text = g_strdup_printf (_("Last synchronized: %s"), time);

    adw_action_row_set_subtitle (ADW_ACTION_ROW (sync_dialog->sync_firefox_account_row), text);

    g_free (text);
    g_free (time);
  }
}

static void
sync_finished_cb (EphySyncService       *service,
                  EphyFirefoxSyncDialog *sync_dialog)
{
  g_assert (EPHY_IS_SYNC_SERVICE (service));
  g_assert (EPHY_IS_FIREFOX_SYNC_DIALOG (sync_dialog));

  gtk_widget_set_sensitive (sync_dialog->sync_now_button, TRUE);
  sync_set_last_sync_time (sync_dialog);
}

static void
sync_sign_in_details_show (EphyFirefoxSyncDialog *sync_dialog,
                           const char            *text)
{
  char *message;

  g_assert (EPHY_IS_FIREFOX_SYNC_DIALOG (sync_dialog));

  message = g_strdup_printf ("<span fgcolor='#e6780b'>%s</span>", text);
  gtk_label_set_markup (GTK_LABEL (sync_dialog->sync_firefox_iframe_label), message);
  gtk_widget_set_visible (sync_dialog->sync_firefox_iframe_label, TRUE);

  g_free (message);
}

static void
sync_sign_in_error_cb (EphySyncService       *service,
                       const char            *error,
                       EphyFirefoxSyncDialog *sync_dialog)
{
  g_assert (EPHY_IS_SYNC_SERVICE (service));
  g_assert (EPHY_IS_FIREFOX_SYNC_DIALOG (sync_dialog));

  /* Display the error message and reload the iframe. */
  sync_sign_in_details_show (sync_dialog, error);
  webkit_web_view_load_uri (sync_dialog->fxa_web_view, FXA_IFRAME_URL);
}

static void
sync_secrets_store_finished_cb (EphySyncService       *service,
                                GError                *error,
                                EphyFirefoxSyncDialog *sync_dialog)
{
  g_assert (EPHY_IS_SYNC_SERVICE (service));
  g_assert (EPHY_IS_FIREFOX_SYNC_DIALOG (sync_dialog));

  if (!error) {
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (sync_dialog->sync_firefox_account_row),
                                   ephy_sync_utils_get_sync_user ());
    gtk_widget_set_visible (sync_dialog->sync_page_group, FALSE);
    gtk_widget_set_visible (sync_dialog->sync_firefox_account_group, TRUE);
    gtk_widget_set_visible (sync_dialog->sync_options_group, TRUE);
    gtk_widget_set_visible (sync_dialog->sync_now_button, TRUE);
  } else {
    /* Display the error message and reload the iframe. */
    sync_sign_in_details_show (sync_dialog, error->message);
    webkit_web_view_load_uri (sync_dialog->fxa_web_view, FXA_IFRAME_URL);
  }
}

static void
sync_message_to_fxa_content (EphyFirefoxSyncDialog *sync_dialog,
                             const char            *web_channel_id,
                             const char            *command,
                             const char            *message_id,
                             JsonObject            *data)
{
  JsonNode *node;
  JsonObject *detail;
  JsonObject *message;
  char *detail_str;
  char *script;
  const char *type;

  g_assert (EPHY_FIREFOX_SYNC_DIALOG (sync_dialog));
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
  webkit_web_view_evaluate_javascript (sync_dialog->fxa_web_view, script, -1, NULL, NULL, NULL, NULL, NULL);

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
                                  JSCValue                 *value,
                                  EphyFirefoxSyncDialog    *sync_dialog)
{
  JsonObject *data = NULL;
  char *message = NULL;
  char *web_channel_id = NULL;
  char *message_id = NULL;
  char *command = NULL;
  char *error_msg = NULL;
  gboolean is_error = FALSE;

  message = jsc_value_to_string (value);
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
    sync_message_to_fxa_content (sync_dialog, web_channel_id, command, message_id, response);
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
    sync_sign_in_details_show (sync_dialog, _("Something went wrong, please try again later."));
    webkit_web_view_load_uri (sync_dialog->fxa_web_view, FXA_IFRAME_URL);
  }
}

static void
sync_open_webmail_clicked_cb (WebKitUserContentManager *manager,
                              JSCValue                 *value)
{
  EphyShell *shell;
  EphyEmbed *embed;
  GtkWindow *window;
  char *url;

  url = jsc_value_to_string (value);
  if (url) {
    /* Open a new tab to the webmail URL. */
    shell = ephy_shell_get_default ();
    window = gtk_application_get_active_window (GTK_APPLICATION (shell));
    embed = ephy_shell_new_tab (shell, EPHY_WINDOW (window),
                                NULL, EPHY_NEW_TAB_JUMP);
    ephy_web_view_load_url (ephy_embed_get_web_view (embed), url);

    g_free (url);
  }
}

static void
sync_setup_firefox_iframe (EphyFirefoxSyncDialog *sync_dialog)
{
  EphyEmbedShell *shell;
  WebKitNetworkSession *network_session;
  WebKitWebContext *embed_context;
  WebKitWebContext *sync_context;
  const char *script;

  if (!sync_dialog->fxa_web_view) {
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

    sync_dialog->fxa_script = webkit_user_script_new (script,
                                                      WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
                                                      WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END,
                                                      NULL, NULL);
    sync_dialog->fxa_manager = webkit_user_content_manager_new ();
    webkit_user_content_manager_add_script (sync_dialog->fxa_manager, sync_dialog->fxa_script);
    g_signal_connect (sync_dialog->fxa_manager,
                      "script-message-received::toChromeMessageHandler",
                      G_CALLBACK (sync_message_from_fxa_content_cb),
                      sync_dialog);
    g_signal_connect (sync_dialog->fxa_manager,
                      "script-message-received::openWebmailClickHandler",
                      G_CALLBACK (sync_open_webmail_clicked_cb),
                      NULL);
    webkit_user_content_manager_register_script_message_handler (sync_dialog->fxa_manager,
                                                                 "toChromeMessageHandler",
                                                                 NULL);
    webkit_user_content_manager_register_script_message_handler (sync_dialog->fxa_manager,
                                                                 "openWebmailClickHandler",
                                                                 NULL);

    shell = ephy_embed_shell_get_default ();
    embed_context = ephy_embed_shell_get_web_context (shell);
    network_session = ephy_embed_shell_get_network_session (shell);
    sync_context = webkit_web_context_new ();
    webkit_web_context_set_preferred_languages (sync_context,
                                                g_object_get_data (G_OBJECT (embed_context), "preferred-languages"));

    sync_dialog->fxa_web_view = WEBKIT_WEB_VIEW (g_object_new (WEBKIT_TYPE_WEB_VIEW,
                                                               "user-content-manager", sync_dialog->fxa_manager,
                                                               "settings", ephy_embed_prefs_get_settings (),
                                                               "web-context", sync_context,
                                                               "network-session", network_session,
                                                               NULL));
    gtk_widget_set_overflow (GTK_WIDGET (sync_dialog->fxa_web_view), GTK_OVERFLOW_HIDDEN);
    gtk_widget_add_css_class (GTK_WIDGET (sync_dialog->fxa_web_view), "card");
    gtk_widget_set_vexpand (GTK_WIDGET (sync_dialog->fxa_web_view), TRUE);
    gtk_widget_set_visible (GTK_WIDGET (sync_dialog->fxa_web_view), TRUE);
    gtk_box_append (GTK_BOX (sync_dialog->sync_firefox_iframe_box),
                    GTK_WIDGET (sync_dialog->fxa_web_view));

    g_object_unref (sync_context);
  }

  webkit_web_view_load_uri (sync_dialog->fxa_web_view, FXA_IFRAME_URL);
  gtk_widget_set_visible (sync_dialog->sync_firefox_iframe_label, FALSE);
}

static void
on_sync_sign_out_button_clicked (GtkWidget             *button,
                                 EphyFirefoxSyncDialog *sync_dialog)
{
  EphySyncService *service = ephy_shell_get_sync_service (ephy_shell_get_default ());

  ephy_sync_service_sign_out (service);

  /* Show Mozilla Accounts iframe. */
  sync_setup_firefox_iframe (sync_dialog);
  gtk_widget_set_visible (sync_dialog->sync_now_button, FALSE);
  gtk_widget_set_visible (sync_dialog->sync_firefox_account_group, FALSE);
  gtk_widget_set_visible (sync_dialog->sync_options_group, FALSE);
  gtk_widget_set_visible (sync_dialog->sync_page_group, TRUE);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (sync_dialog->sync_firefox_account_row), NULL);
}

static void
on_sync_sync_now_button_clicked (GtkWidget             *button,
                                 EphyFirefoxSyncDialog *sync_dialog)
{
  EphySyncService *service = ephy_shell_get_sync_service (ephy_shell_get_default ());

  gtk_widget_set_sensitive (button, FALSE);
  ephy_sync_service_sync (service);
}

static void
on_sync_synced_tabs_button_clicked (GtkWidget             *button,
                                    EphyFirefoxSyncDialog *sync_dialog)
{
  EphyOpenTabsManager *manager;
  SyncedTabsDialog *synced_tabs_dialog;

  manager = ephy_shell_get_open_tabs_manager (ephy_shell_get_default ());
  synced_tabs_dialog = synced_tabs_dialog_new (manager);
  gtk_window_set_transient_for (GTK_WINDOW (synced_tabs_dialog), GTK_WINDOW (sync_dialog));
  gtk_window_set_modal (GTK_WINDOW (synced_tabs_dialog), TRUE);
  gtk_window_present (GTK_WINDOW (synced_tabs_dialog));
}

static void
on_sync_device_name_change_button_clicked (GtkWidget             *button,
                                           EphyFirefoxSyncDialog *sync_dialog)
{
  gtk_widget_set_sensitive (GTK_WIDGET (sync_dialog->sync_device_name_entry), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (sync_dialog->sync_device_name_change_button), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (sync_dialog->sync_device_name_save_button), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (sync_dialog->sync_device_name_cancel_button), TRUE);
}

static void
on_sync_device_name_save_button_clicked (GtkWidget             *button,
                                         EphyFirefoxSyncDialog *sync_dialog)
{
  EphySyncService *service = ephy_shell_get_sync_service (ephy_shell_get_default ());
  const char *text;

  text = gtk_editable_get_text (GTK_EDITABLE (sync_dialog->sync_device_name_entry));
  if (!g_strcmp0 (text, "")) {
    char *name = ephy_sync_utils_get_device_name ();
    gtk_editable_set_text (GTK_EDITABLE (sync_dialog->sync_device_name_entry), name);
    g_free (name);
  } else {
    ephy_sync_service_update_device_name (service, text);
  }

  gtk_widget_set_sensitive (GTK_WIDGET (sync_dialog->sync_device_name_entry), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (sync_dialog->sync_device_name_change_button), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (sync_dialog->sync_device_name_save_button), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (sync_dialog->sync_device_name_cancel_button), FALSE);
}

static void
on_sync_device_name_cancel_button_clicked (GtkWidget             *button,
                                           EphyFirefoxSyncDialog *sync_dialog)
{
  char *name;

  name = ephy_sync_utils_get_device_name ();
  gtk_editable_set_text (GTK_EDITABLE (sync_dialog->sync_device_name_entry), name);

  gtk_widget_set_sensitive (GTK_WIDGET (sync_dialog->sync_device_name_entry), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (sync_dialog->sync_device_name_change_button), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (sync_dialog->sync_device_name_save_button), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (sync_dialog->sync_device_name_cancel_button), FALSE);

  g_free (name);
}

static gchar *
get_sync_frequency_minutes_name (EphySyncFrequency *value)
{
  guint n_minutes = value->frequency;
  const char *minutes_text = ngettext ("%u min", "%u mins", n_minutes);

  return g_strdup_printf (minutes_text, n_minutes);
}

static void
prefs_sync_page_finalize (GObject *object)
{
  EphyFirefoxSyncDialog *sync_dialog = EPHY_FIREFOX_SYNC_DIALOG (object);

  if (sync_dialog->fxa_web_view) {
    webkit_user_content_manager_unregister_script_message_handler (sync_dialog->fxa_manager,
                                                                   "toChromeMessageHandler",
                                                                   NULL);
    webkit_user_content_manager_unregister_script_message_handler (sync_dialog->fxa_manager,
                                                                   "openWebmailClickHandler",
                                                                   NULL);
    webkit_user_script_unref (sync_dialog->fxa_script);
    g_object_unref (sync_dialog->fxa_manager);
  }

  G_OBJECT_CLASS (ephy_firefox_sync_dialog_parent_class)->finalize (object);
}

static void
ephy_firefox_sync_dialog_class_init (EphyFirefoxSyncDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/firefox-sync-dialog.ui");

  object_class->finalize = prefs_sync_page_finalize;

  gtk_widget_class_bind_template_child (widget_class, EphyFirefoxSyncDialog, sync_page_group);
  gtk_widget_class_bind_template_child (widget_class, EphyFirefoxSyncDialog, sync_firefox_iframe_box);
  gtk_widget_class_bind_template_child (widget_class, EphyFirefoxSyncDialog, sync_firefox_iframe_label);
  gtk_widget_class_bind_template_child (widget_class, EphyFirefoxSyncDialog, sync_firefox_account_group);
  gtk_widget_class_bind_template_child (widget_class, EphyFirefoxSyncDialog, sync_firefox_account_row);
  gtk_widget_class_bind_template_child (widget_class, EphyFirefoxSyncDialog, sync_options_group);
  gtk_widget_class_bind_template_child (widget_class, EphyFirefoxSyncDialog, sync_bookmarks_row);
  gtk_widget_class_bind_template_child (widget_class, EphyFirefoxSyncDialog, sync_passwords_row);
  gtk_widget_class_bind_template_child (widget_class, EphyFirefoxSyncDialog, sync_history_row);
  gtk_widget_class_bind_template_child (widget_class, EphyFirefoxSyncDialog, sync_open_tabs_row);
  gtk_widget_class_bind_template_child (widget_class, EphyFirefoxSyncDialog, sync_frequency_row);
  gtk_widget_class_bind_template_child (widget_class, EphyFirefoxSyncDialog, sync_now_button);
  gtk_widget_class_bind_template_child (widget_class, EphyFirefoxSyncDialog, synced_tabs_button);
  gtk_widget_class_bind_template_child (widget_class, EphyFirefoxSyncDialog, sync_device_name_entry);
  gtk_widget_class_bind_template_child (widget_class, EphyFirefoxSyncDialog, sync_device_name_change_button);
  gtk_widget_class_bind_template_child (widget_class, EphyFirefoxSyncDialog, sync_device_name_save_button);
  gtk_widget_class_bind_template_child (widget_class, EphyFirefoxSyncDialog, sync_device_name_cancel_button);

  gtk_widget_class_bind_template_callback (widget_class, on_sync_sign_out_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_sync_sync_now_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_sync_synced_tabs_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_sync_device_name_change_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_sync_device_name_save_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_sync_device_name_cancel_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, get_sync_frequency_minutes_name);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);
}

static gboolean
sync_frequency_get_mapping (GValue   *value,
                            GVariant *variant,
                            gpointer  user_data)
{
  uint minutes = g_variant_get_uint32 (variant);

  for (guint i = 0; i < (guint)G_N_ELEMENTS (sync_frequency_minutes); i++) {
    if (sync_frequency_minutes[i] != minutes)
      continue;

    g_value_set_uint (value, i);

    return TRUE;
  }

  return FALSE;
}

static GVariant *
sync_frequency_set_mapping (const GValue       *value,
                            const GVariantType *expected_type,
                            gpointer            user_data)
{
  guint i = g_value_get_uint (value);

  if (i >= (guint)G_N_ELEMENTS (sync_frequency_minutes))
    return NULL;

  return g_variant_new_uint32 (sync_frequency_minutes[i]);
}

static GListModel *
create_sync_frequency_minutes_model ()
{
  GListStore *list_store = g_list_store_new (EPHY_TYPE_SYNC_FREQUENCY);
  guint i;

  for (i = 0; i < G_N_ELEMENTS (sync_frequency_minutes); i++) {
    g_autoptr (EphySyncFrequency) frequency =
      ephy_sync_frequency_new (sync_frequency_minutes[i]);

    g_list_store_insert (list_store, i, frequency);
  }

  return G_LIST_MODEL (list_store);
}

void
ephy_firefox_sync_dialog_setup (EphyFirefoxSyncDialog *sync_dialog)
{
  EphySyncService *service = ephy_shell_get_sync_service (ephy_shell_get_default ());
  GSettings *sync_settings = ephy_settings_get (EPHY_PREFS_SYNC_SCHEMA);
  char *user = ephy_sync_utils_get_sync_user ();
  char *name = ephy_sync_utils_get_device_name ();
  g_autoptr (GListModel) sync_frequency_minutes_model = create_sync_frequency_minutes_model ();

  gtk_editable_set_text (GTK_EDITABLE (sync_dialog->sync_device_name_entry), name);

  if (!user) {
    sync_setup_firefox_iframe (sync_dialog);
    gtk_widget_set_visible (sync_dialog->sync_now_button, FALSE);
    gtk_widget_set_visible (sync_dialog->sync_firefox_account_group, FALSE);
    gtk_widget_set_visible (sync_dialog->sync_options_group, FALSE);
  } else {
    sync_set_last_sync_time (sync_dialog);
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (sync_dialog->sync_firefox_account_row), user);
    gtk_widget_set_visible (sync_dialog->sync_page_group, FALSE);
  }

  g_settings_bind (sync_settings,
                   EPHY_PREFS_SYNC_BOOKMARKS_ENABLED,
                   sync_dialog->sync_bookmarks_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (sync_settings,
                   EPHY_PREFS_SYNC_PASSWORDS_ENABLED,
                   sync_dialog->sync_passwords_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (sync_settings,
                   EPHY_PREFS_SYNC_HISTORY_ENABLED,
                   sync_dialog->sync_history_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (sync_settings,
                   EPHY_PREFS_SYNC_OPEN_TABS_ENABLED,
                   sync_dialog->sync_open_tabs_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  adw_combo_row_set_model (ADW_COMBO_ROW (sync_dialog->sync_frequency_row),
                           sync_frequency_minutes_model);
  g_settings_bind_with_mapping (sync_settings,
                                EPHY_PREFS_SYNC_FREQUENCY,
                                sync_dialog->sync_frequency_row,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                sync_frequency_get_mapping,
                                sync_frequency_set_mapping,
                                NULL, NULL);

  g_object_bind_property (sync_dialog->sync_open_tabs_row, "active",
                          sync_dialog->synced_tabs_button, "sensitive",
                          G_BINDING_SYNC_CREATE);

  g_signal_connect_object (service, "sync-secrets-store-finished",
                           G_CALLBACK (sync_secrets_store_finished_cb),
                           sync_dialog, 0);
  g_signal_connect_object (service, "sync-sign-in-error",
                           G_CALLBACK (sync_sign_in_error_cb),
                           sync_dialog, 0);
  g_signal_connect_object (service, "sync-finished",
                           G_CALLBACK (sync_finished_cb),
                           sync_dialog, 0);
  g_signal_connect_object (sync_dialog->sync_bookmarks_row, "notify::active",
                           G_CALLBACK (sync_collection_toggled_cb),
                           sync_dialog, 0);
  g_signal_connect_object (sync_dialog->sync_passwords_row, "notify::active",
                           G_CALLBACK (sync_collection_toggled_cb),
                           sync_dialog, 0);
  g_signal_connect_object (sync_dialog->sync_history_row, "notify::active",
                           G_CALLBACK (sync_collection_toggled_cb),
                           sync_dialog, 0);
  g_signal_connect_object (sync_dialog->sync_open_tabs_row, "notify::active",
                           G_CALLBACK (sync_collection_toggled_cb),
                           sync_dialog, 0);

  g_free (user);
  g_free (name);
}

static void
ephy_firefox_sync_dialog_init (EphyFirefoxSyncDialog *sync_dialog)
{
  gtk_widget_init_template (GTK_WIDGET (sync_dialog));
  ephy_firefox_sync_dialog_setup (sync_dialog);
}

GtkWidget *
ephy_firefox_sync_dialog_new ()
{
  return GTK_WIDGET (g_object_new (EPHY_TYPE_FIREFOX_SYNC_DIALOG, NULL));
}
