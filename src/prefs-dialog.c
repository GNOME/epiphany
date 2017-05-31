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
#include "ephy-gui.h"
#include "ephy-langs.h"
#include "ephy-prefs.h"
#include "ephy-search-engine-dialog.h"
#include "ephy-session.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-sync-service.h"
#include "ephy-uri-tester-shared.h"
#include "clear-data-dialog.h"
#include "cookies-dialog.h"
#include "languages.h"
#include "passwords-dialog.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <JavaScriptCore/JavaScript.h>
#include <json-glib/json-glib.h>
#include <string.h>

#define DOWNLOAD_BUTTON_WIDTH   8
#define FXA_IFRAME_URL "https://accounts.firefox.com/signin?service=sync&context=fx_desktop_v3"

enum {
  COL_LANG_NAME,
  COL_LANG_CODE
};

struct _PrefsDialog {
  GtkDialog parent_instance;

  /* general */
  GtkWidget *homepage_box;
  GtkWidget *new_tab_homepage_radiobutton;
  GtkWidget *blank_homepage_radiobutton;
  GtkWidget *custom_homepage_radiobutton;
  GtkWidget *custom_homepage_entry;
  GtkWidget *download_button_hbox;
  GtkWidget *download_button_label;
  GtkWidget *automatic_downloads_checkbutton;
  GtkWidget *search_box;
  GtkWidget *session_box;
  GtkWidget *restore_session_checkbutton;
  GtkWidget *popups_allow_checkbutton;
  GtkWidget *adblock_allow_checkbutton;
  GtkWidget *enable_plugins_checkbutton;

  /* fonts */
  GtkWidget *use_gnome_fonts_checkbutton;
  GtkWidget *custom_fonts_table;
  GtkWidget *sans_fontbutton;
  GtkWidget *serif_fontbutton;
  GtkWidget *mono_fontbutton;
  GtkWidget *css_checkbox;
  GtkWidget *css_edit_button;

  /* stored data */
  GtkWidget *always;
  GtkWidget *no_third_party;
  GtkWidget *never;
  GtkWidget *remember_passwords_checkbutton;
  GtkWidget *do_not_track_checkbutton;
  GtkWidget *clear_personal_data_button;

  /* language */
  GtkTreeView *lang_treeview;
  GtkWidget *lang_add_button;
  GtkWidget *lang_remove_button;
  GtkWidget *lang_up_button;
  GtkWidget *lang_down_button;
  GtkWidget *enable_spell_checking_checkbutton;

  GtkDialog *add_lang_dialog;
  GtkTreeView *add_lang_treeview;
  GtkTreeModel *lang_model;

  GHashTable *iso_639_table;
  GHashTable *iso_3166_table;

  /* sync */
  EphySyncService *sync_service;
  GtkWidget *sync_page_box;
  GtkWidget *sync_firefox_iframe_box;
  GtkWidget *sync_firefox_iframe_label;
  GtkWidget *sync_firefox_account_box;
  GtkWidget *sync_firefox_account_label;
  GtkWidget *sync_sign_out_button;
  GtkWidget *sync_options_box;
  GtkWidget *sync_with_firefox_checkbutton;
  GtkWidget *sync_bookmarks_checkbutton;
  GtkWidget *sync_passwords_checkbutton;
  GtkWidget *sync_history_checkbutton;
  GtkWidget *sync_frequency_5_min_radiobutton;
  GtkWidget *sync_frequency_15_min_radiobutton;
  GtkWidget *sync_frequency_30_min_radiobutton;
  GtkWidget *sync_frequency_60_min_radiobutton;
  GtkWidget *sync_now_button;
  gboolean sync_was_signed_in;

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

  g_hash_table_destroy (dialog->iso_639_table);
  g_hash_table_destroy (dialog->iso_3166_table);

  if (dialog->fxa_web_view != NULL) {
    webkit_user_content_manager_unregister_script_message_handler (dialog->fxa_manager,
                                                                   "toChromeMessageHandler");
    webkit_user_script_unref (dialog->fxa_script);
    g_object_unref (dialog->fxa_manager);
  }

  if (ephy_sync_service_is_signed_in (dialog->sync_service) && !dialog->sync_was_signed_in)
    ephy_sync_service_start_periodical_sync (dialog->sync_service);

  G_OBJECT_CLASS (prefs_dialog_parent_class)->finalize (object);
}

static void
sync_collection_toggled_cb (GtkToggleButton *button,
                            PrefsDialog     *dialog)
{
  EphySynchronizableManager *manager = NULL;

  if (GTK_WIDGET (button) == dialog->sync_bookmarks_checkbutton)
    manager = EPHY_SYNCHRONIZABLE_MANAGER (ephy_shell_get_bookmarks_manager (ephy_shell_get_default ()));
  else if (GTK_WIDGET (button) == dialog->sync_passwords_checkbutton)
    manager = EPHY_SYNCHRONIZABLE_MANAGER (ephy_shell_get_password_manager (ephy_shell_get_default ()));
  else if (GTK_WIDGET (button) == dialog->sync_history_checkbutton)
    manager = EPHY_SYNCHRONIZABLE_MANAGER (ephy_shell_get_history_manager (ephy_shell_get_default ()));
  else
    g_assert_not_reached ();

  if (gtk_toggle_button_get_active (button)) {
    ephy_sync_service_register_manager (dialog->sync_service, manager);
  } else {
    ephy_sync_service_unregister_manager (dialog->sync_service, manager);
    ephy_synchronizable_manager_set_is_initial_sync (manager, TRUE);
  }
}

static void
sync_finished_cb (EphySyncService *service,
                  PrefsDialog     *dialog)
{
  g_assert (EPHY_IS_SYNC_SERVICE (service));
  g_assert (EPHY_IS_PREFS_DIALOG (dialog));

  gtk_widget_set_sensitive (dialog->sync_now_button, TRUE);
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
  EphyBookmarksManager *bookmarks_manager;
  EphyPasswordManager *password_manager;
  EphyHistoryManager *history_manager;

  g_assert (EPHY_IS_SYNC_SERVICE (service));
  g_assert (EPHY_IS_PREFS_DIALOG (dialog));

  if (!error) {
    char *text;
    char *user;

    /* Show sync options panel. */
    user = g_strdup_printf ("<b>%s</b>", ephy_sync_service_get_sync_user (service));
    /* Translators: the %s refers to the email of the currently logged in user. */
    text = g_strdup_printf (_("Currently logged in as %s"), user);
    gtk_label_set_markup (GTK_LABEL (dialog->sync_firefox_account_label), text);
    gtk_container_remove (GTK_CONTAINER (dialog->sync_page_box),
                          dialog->sync_firefox_iframe_box);
    gtk_box_pack_start (GTK_BOX (dialog->sync_page_box),
                        dialog->sync_firefox_account_box,
                        FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (dialog->sync_page_box),
                        dialog->sync_options_box,
                        FALSE, FALSE, 0);

    g_settings_set_string (EPHY_SETTINGS_SYNC,
                           EPHY_PREFS_SYNC_USER,
                           ephy_sync_service_get_sync_user (service));

    if (g_settings_get_boolean (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_BOOKMARKS_ENABLED)) {
      bookmarks_manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
      ephy_sync_service_register_manager (service, EPHY_SYNCHRONIZABLE_MANAGER (bookmarks_manager));
    }
    if (g_settings_get_boolean (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_PASSWORDS_ENABLED)) {
      password_manager = ephy_shell_get_password_manager (ephy_shell_get_default ());
      ephy_sync_service_register_manager (service, EPHY_SYNCHRONIZABLE_MANAGER (password_manager));
    }
    if (g_settings_get_boolean (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_HISTORY_ENABLED)) {
      history_manager = ephy_shell_get_history_manager (ephy_shell_get_default ());
      ephy_sync_service_register_manager (service, EPHY_SYNCHRONIZABLE_MANAGER (history_manager));
    }

    g_free (text);
    g_free (user);
  } else {
    /* Display the error message and reload the iframe. */
    sync_sign_in_details_show (dialog, error->message);
    webkit_web_view_load_uri (dialog->fxa_web_view, FXA_IFRAME_URL);
  }
}

static void
sync_send_message_to_content (PrefsDialog *dialog,
                              const char  *channel_id,
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

  message = json_object_new ();
  json_object_set_string_member (message, "command", command);
  json_object_set_string_member (message, "messageId", message_id);
  json_object_set_object_member (message, "data", json_object_ref (data));
  detail = json_object_new ();
  json_object_set_string_member (detail, "id", channel_id);
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

static void
sync_fxa_server_message_cb (WebKitUserContentManager *manager,
                            WebKitJavascriptResult   *result,
                            PrefsDialog              *dialog)
{
  JsonNode *node = NULL;
  JsonObject *json = NULL;
  JsonObject *detail = NULL;
  JsonObject *message = NULL;
  JsonObject *data = NULL;
  GError *error = NULL;
  char *json_string = NULL;
  const char *type;
  const char *command;
  const char *email;
  const char *uid;
  const char *session_token;
  const char *key_fetch_token;
  const char *unwrap_b_key;

  json_string = ephy_embed_utils_get_js_result_as_string (result);
  if (!json_string) {
    g_warning ("Failed to get JavaScript result as string");
    goto out_error;
  }
  node = json_from_string (json_string, &error);
  if (error) {
    g_warning ("Response is not a valid JSON: %s", error->message);
    goto out_error;
  }
  json = json_node_get_object (node);
  if (!json) {
    g_warning ("JSON node does not hold a JSON object");
    goto out_error;
  }
  type = json_object_get_string_member (json, "type");
  if (!type) {
    g_warning ("JSON object has missing or invalid 'type' member");
    goto out_error;
  }
  /* The only message type expected is WebChannelMessageToChrome. */
  if (g_strcmp0 (type, "WebChannelMessageToChrome")) {
    g_warning ("Unknown command type: %s", type);
    goto out_error;
  }
  detail = json_object_get_object_member (json, "detail");
  if (!detail) {
    g_warning ("JSON object has missing or invalid 'detail' member");
    goto out_error;
  }
  message = json_object_get_object_member (detail, "message");
  if (!message) {
    g_warning ("JSON object has missing or invalid 'message' member");
    goto out_error;
  }
  command = json_object_get_string_member (message, "command");
  if (!command) {
    g_warning ("JSON object has missing or invalid 'command' member");
    goto out_error;
  }

  if (!g_strcmp0 (command, "fxaccounts:loaded")) {
    LOG ("Firefox Accounts iframe loaded");
    goto out_no_error;
  }
  if (!g_strcmp0 (command, "fxaccounts:can_link_account")) {
    /* We need to confirm a relink. */
    data = json_object_new ();
    json_object_set_boolean_member (data, "ok", TRUE);
    sync_send_message_to_content (dialog,
                                  json_object_get_string_member (detail, "id"),
                                  "fxaccounts:can_link_account",
                                  json_object_get_string_member (message, "messageId"),
                                  data);
    json_object_unref (data);
    goto out_no_error;
  }
  if (g_strcmp0 (command, "fxaccounts:login")) {
    g_warning ("Unexepected command: %s", command);
    goto out_error;
  }

  /* Login command. */
  gtk_widget_set_visible (dialog->sync_firefox_iframe_label, FALSE);

  data = json_object_get_object_member (message, "data");
  if (!data) {
    g_warning ("JSON object has invalid 'data' member");
    goto out_error;
  }
  email = json_object_get_string_member (data, "email");
  uid = json_object_get_string_member (data, "uid");
  session_token = json_object_get_string_member (data, "sessionToken");
  key_fetch_token = json_object_get_string_member (data, "keyFetchToken");
  unwrap_b_key = json_object_get_string_member (data, "unwrapBKey");
  if (!email || !uid || !session_token || !key_fetch_token || !unwrap_b_key) {
    g_warning ("JSON object has missing or invalid members");
    goto out_error;
  }
  if (!json_object_has_member (data, "verified") ||
      !JSON_NODE_HOLDS_VALUE (json_object_get_member (data, "verified"))) {
    g_warning ("JSON object has missing or invalid 'verified' member");
    goto out_error;
  }

  if (!json_object_get_boolean_member (data, "verified"))
    sync_sign_in_details_show (dialog, _("Please don’t leave this page until "
                                         "you have completed the verification."));
  ephy_sync_service_do_sign_in (dialog->sync_service, email, uid,
                                session_token, key_fetch_token, unwrap_b_key);
  goto out_no_error;

out_error:
  sync_sign_in_details_show (dialog, _("Something went wrong, please try again."));
  webkit_web_view_load_uri (dialog->fxa_web_view, FXA_IFRAME_URL);
out_no_error:
  if (node)
    json_node_unref (node);
  if (error)
    g_error_free (error);
  g_free (json_string);
}

static void
sync_setup_firefox_iframe (PrefsDialog *dialog)
{
  EphyEmbedShell *shell;
  WebKitWebContext *embed_context;
  WebKitWebContext *sync_context;
  const char *script;

  if (!dialog->fxa_web_view) {
    script = "function handleToChromeMessage(evt) {"
             "  let e = JSON.stringify({type: evt.type, detail: evt.detail});"
             "  window.webkit.messageHandlers.toChromeMessageHandler.postMessage(e);"
             "};"
             "window.addEventListener(\"WebChannelMessageToChrome\", handleToChromeMessage);";
    dialog->fxa_script = webkit_user_script_new (script,
                                                 WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
                                                 WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END,
                                                 NULL, NULL);
    dialog->fxa_manager = webkit_user_content_manager_new ();
    webkit_user_content_manager_add_script (dialog->fxa_manager, dialog->fxa_script);
    g_signal_connect (dialog->fxa_manager,
                      "script-message-received::toChromeMessageHandler",
                      G_CALLBACK (sync_fxa_server_message_cb),
                      dialog);
    webkit_user_content_manager_register_script_message_handler (dialog->fxa_manager,
                                                                 "toChromeMessageHandler");

    shell = ephy_embed_shell_get_default ();
    embed_context = ephy_embed_shell_get_web_context (shell);
    sync_context = webkit_web_context_new ();
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

  ephy_sync_service_do_sign_out (dialog->sync_service);

  /* Show Firefox Accounts iframe. */
  sync_setup_firefox_iframe (dialog);
  gtk_container_remove (GTK_CONTAINER (dialog->sync_page_box),
                        dialog->sync_firefox_account_box);
  gtk_container_remove (GTK_CONTAINER (dialog->sync_page_box),
                        dialog->sync_options_box);
  gtk_box_pack_start (GTK_BOX (dialog->sync_page_box),
                      dialog->sync_firefox_iframe_box,
                      FALSE, FALSE, 0);

  dialog->sync_was_signed_in = FALSE;
}

static void
on_sync_sync_now_button_clicked (GtkWidget   *button,
                                 PrefsDialog *dialog)
{
  gtk_widget_set_sensitive (button, FALSE);
  ephy_sync_service_do_sync (dialog->sync_service);
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

  password_manager = ephy_shell_get_password_manager (ephy_shell_get_default ());
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

static void
prefs_dialog_class_init (PrefsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = prefs_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/prefs-dialog.ui");
  /* general */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, homepage_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, new_tab_homepage_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, blank_homepage_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, custom_homepage_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, custom_homepage_entry);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, automatic_downloads_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, search_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, session_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, restore_session_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, popups_allow_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, adblock_allow_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, enable_plugins_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, download_button_hbox);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, download_button_label);

  /* fonts */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, use_gnome_fonts_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, custom_fonts_table);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sans_fontbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, serif_fontbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, mono_fontbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, css_checkbox);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, css_edit_button);

  /* stored data */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, always);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, no_third_party);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, never);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, remember_passwords_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, do_not_track_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, clear_personal_data_button);

  /* language */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, lang_treeview);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, lang_add_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, lang_remove_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, lang_up_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, lang_down_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, enable_spell_checking_checkbutton);

  /* sync */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_page_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_firefox_iframe_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_firefox_iframe_label);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_firefox_account_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_firefox_account_label);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_sign_out_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_options_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_with_firefox_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_bookmarks_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_passwords_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_history_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_frequency_5_min_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_frequency_15_min_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_frequency_30_min_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_frequency_60_min_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_now_button);

  gtk_widget_class_bind_template_callback (widget_class, on_manage_cookies_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_manage_passwords_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_search_engine_dialog_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_sync_sign_out_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_sync_sync_now_button_clicked);
}

static void
css_edit_button_clicked_cb (GtkWidget   *button,
                            PrefsDialog *pd)
{
  GFile *css_file;

  css_file = g_file_new_for_path (g_build_filename (ephy_dot_dir (),
                                                    USER_STYLESHEET_FILENAME,
                                                    NULL));

  ephy_file_launch_handler ("text/plain", css_file,
                            gtk_get_current_event_time ());
  g_object_unref (css_file);
}

static void
language_editor_add (PrefsDialog *pd,
                     const char  *code,
                     const char  *desc)
{
  GtkTreeIter iter;

  g_return_if_fail (code != NULL && desc != NULL);

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

  g_return_if_fail (dialog != NULL);

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

static char *
get_name_for_lang_code (PrefsDialog *pd,
                        const char  *code)
{
  char **str;
  char *name;
  const char *langname, *localename;
  int len;

  str = g_strsplit (code, "-", -1);
  len = g_strv_length (str);
  g_return_val_if_fail (len != 0, NULL);

  langname = (const char *)g_hash_table_lookup (pd->iso_639_table, str[0]);

  if (len == 1 && langname != NULL) {
    name = g_strdup (dgettext (ISO_639_DOMAIN, langname));
  } else if (len == 2 && langname != NULL) {
    localename = (const char *)g_hash_table_lookup (pd->iso_3166_table, str[1]);

    if (localename != NULL) {
      /* Translators: the first %s is the language name, and the
       * second %s is the locale name. Example:
       * "French (France)"
       */
      name = g_strdup_printf (C_("language", "%s (%s)"),
                              dgettext (ISO_639_DOMAIN, langname),
                              dgettext (ISO_3166_DOMAIN, localename));
    } else {
      name = g_strdup_printf (C_("language", "%s (%s)"),
                              dgettext (ISO_639_DOMAIN, langname), str[1]);
    }
  } else {
    /* Translators: this refers to a user-define language code
     * (one which isn't in our built-in list).
     */
    name = g_strdup_printf (C_("language", "User defined (%s)"), code);
  }

  g_strfreev (str);

  return name;
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
  guint i;
  GtkBuilder *builder;

  builder = gtk_builder_new_from_resource ("/org/gnome/epiphany/gtk/prefs-lang-dialog.ui");
  ad = GTK_WIDGET (gtk_builder_get_object (builder, "add_language_dialog"));
  add_button = GTK_WIDGET (gtk_builder_get_object (builder, "add_button"));
  treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder, "languages_treeview"));
  dialog->add_lang_treeview = treeview;

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

  for (i = 0; i < G_N_ELEMENTS (languages); i++) {
    const char *code = languages[i];
    char *name;

    name = get_name_for_lang_code (dialog, code);

    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
                        COL_LANG_NAME, name,
                        COL_LANG_CODE, code,
                        -1);
    g_free (name);
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

  dialog->iso_639_table = ephy_langs_iso_639_table ();
  dialog->iso_3166_table = ephy_langs_iso_3166_table ();

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
    const char *code = (const char *)list[i];

    if (strcmp (code, "system") == 0) {
      add_system_language_entry (store);
    } else if (code[0] != '\0') {
      char *text;

      text = get_name_for_lang_code (dialog, code);
      language_editor_add (dialog, code, text);
      g_free (text);
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
  gtk_label_set_mnemonic_widget (GTK_LABEL (dialog->download_button_label), button);
  gtk_box_pack_start (GTK_BOX (dialog->download_button_hbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);

  g_settings_bind_writable (EPHY_SETTINGS_STATE,
                            EPHY_PREFS_STATE_DOWNLOAD_DIR,
                            button, "sensitive", FALSE);
  g_free (dir);
}

static void
prefs_dialog_response_cb (GtkDialog *widget,
                          int        response,
                          GtkDialog *dialog)
{
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
do_not_track_button_clicked_cb (GtkWidget   *button,
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
  if (!g_value_get_boolean (value))
    return NULL;
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
  if (!g_value_get_boolean (value))
    return NULL;
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
  PrefsDialog *dialog = user_data;
  const char *setting;

  if (!g_value_get_boolean (value)) {
    gtk_widget_set_sensitive (dialog->custom_homepage_entry, FALSE);
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
  g_settings_set_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_HOMEPAGE_URL,
                         gtk_entry_get_text (entry));
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

  settings = ephy_settings_get (EPHY_PREFS_SCHEMA);
  web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);

  g_settings_bind_with_mapping (settings,
                                EPHY_PREFS_HOMEPAGE_URL,
                                dialog->new_tab_homepage_radiobutton,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                new_tab_homepage_get_mapping,
                                new_tab_homepage_set_mapping,
                                NULL,
                                NULL);
  g_settings_bind_with_mapping (settings,
                                EPHY_PREFS_HOMEPAGE_URL,
                                dialog->blank_homepage_radiobutton,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                blank_homepage_get_mapping,
                                blank_homepage_set_mapping,
                                NULL,
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

  g_settings_bind (settings,
                   EPHY_PREFS_AUTO_DOWNLOADS,
                   dialog->automatic_downloads_checkbutton,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind_with_mapping (settings,
                                EPHY_PREFS_RESTORE_SESSION_POLICY,
                                dialog->restore_session_checkbutton,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                restore_session_get_mapping,
                                restore_session_set_mapping,
                                NULL, NULL);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_POPUPS,
                   dialog->popups_allow_checkbutton,
                   "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_PLUGINS,
                   dialog->enable_plugins_checkbutton,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_ADBLOCK,
                   dialog->adblock_allow_checkbutton,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_ADBLOCK,
                   dialog->do_not_track_checkbutton,
                   "sensitive",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_DO_NOT_TRACK,
                   dialog->do_not_track_checkbutton,
                   "active",
                   /* Teensy hack: don't override the previous binding. */
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_signal_connect (dialog->do_not_track_checkbutton,
                    "clicked",
                    G_CALLBACK (do_not_track_button_clicked_cb),
                    dialog);

  create_download_path_button (dialog);
}

static void
setup_fonts_page (PrefsDialog *dialog)
{
  GSettings *web_settings;

  web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_USE_GNOME_FONTS,
                   dialog->use_gnome_fonts_checkbutton,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_USE_GNOME_FONTS,
                   dialog->custom_fonts_table,
                   "sensitive",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_INVERT_BOOLEAN);
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
                   dialog->css_checkbox,
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
                   dialog->remember_passwords_checkbutton,
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
                   dialog->enable_spell_checking_checkbutton,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  create_language_section (dialog);
}

static void
setup_sync_page (PrefsDialog *dialog)
{
  GSettings *sync_settings;
  char *user;
  char *text;

  sync_settings = ephy_settings_get (EPHY_PREFS_SYNC_SCHEMA);
  dialog->sync_service = ephy_shell_get_sync_service (ephy_shell_get_default ());
  dialog->sync_was_signed_in = ephy_sync_service_is_signed_in (dialog->sync_service);

  if (!dialog->sync_was_signed_in) {
    sync_setup_firefox_iframe (dialog);
    gtk_container_remove (GTK_CONTAINER (dialog->sync_page_box),
                          dialog->sync_firefox_account_box);
    gtk_container_remove (GTK_CONTAINER (dialog->sync_page_box),
                          dialog->sync_options_box);
  } else {
    gtk_container_remove (GTK_CONTAINER (dialog->sync_page_box),
                          dialog->sync_firefox_iframe_box);

    user = g_strdup_printf ("<b>%s</b>",
                            ephy_sync_service_get_sync_user (dialog->sync_service));
    /* Translators: the %s refers to the email of the currently logged in user. */
    text = g_strdup_printf (_("Currently logged in as %s"), user);
    gtk_label_set_markup (GTK_LABEL (dialog->sync_firefox_account_label), text);

    g_free (text);
    g_free (user);
  }

  g_signal_connect_object (dialog->sync_service, "sync-secrets-store-finished",
                           G_CALLBACK (sync_secrets_store_finished_cb),
                           dialog, 0);
  g_signal_connect_object (dialog->sync_service, "sync-sign-in-error",
                           G_CALLBACK (sync_sign_in_error_cb),
                           dialog, 0);
  g_signal_connect_object (dialog->sync_service, "sync-finished",
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

  g_settings_bind (sync_settings,
                   EPHY_PREFS_SYNC_WITH_FIREFOX,
                   dialog->sync_with_firefox_checkbutton,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
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
}

static void
prefs_dialog_init (PrefsDialog *dialog)
{
  EphyEmbedShellMode mode;

  gtk_widget_init_template (GTK_WIDGET (dialog));

  mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ());
  gtk_widget_set_visible (dialog->homepage_box,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
  gtk_widget_set_visible (dialog->search_box,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
  gtk_widget_set_visible (dialog->automatic_downloads_checkbutton,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
  gtk_widget_set_visible (dialog->session_box,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
  gtk_widget_set_visible (dialog->do_not_track_checkbutton,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);

  setup_general_page (dialog);
  setup_fonts_page (dialog);
  setup_stored_data_page (dialog);
  setup_language_page (dialog);
  if (mode != EPHY_EMBED_SHELL_MODE_APPLICATION)
    setup_sync_page (dialog);

  ephy_gui_ensure_window_group (GTK_WINDOW (dialog));
  g_signal_connect (dialog, "response",
                    G_CALLBACK (prefs_dialog_response_cb), dialog);
}
