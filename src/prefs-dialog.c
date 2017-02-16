/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 200-2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *  Copyright © 2010 Igalia S.L.
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

#ifdef ENABLE_SYNC
#include "ephy-sync-crypto.h"
#include "ephy-sync-secret.h"
#include "ephy-sync-service.h"
#endif

#define DOWNLOAD_BUTTON_WIDTH   8
#define FXA_IFRAME_URL "https://accounts.firefox.com/signin?service=sync&context=fx_ios_v1"

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

#ifdef ENABLE_SYNC
  /* sync */
  GtkWidget *sync_authenticate_box;
  GtkWidget *sync_sign_in_box;
  GtkWidget *sync_sign_in_details;
  GtkWidget *sync_sign_out_box;
  GtkWidget *sync_sign_out_details;
  GtkWidget *sync_sign_out_button;

  WebKitWebView *fxa_web_view;
  WebKitUserContentManager *fxa_manager;
  WebKitUserScript *fxa_script;
  guint fxa_id;
#endif
  GtkWidget *notebook;
};

#ifdef ENABLE_SYNC
typedef struct {
  PrefsDialog *dialog;
  char        *email;
  char        *uid;
  char        *sessionToken;
  char        *keyFetchToken;
  char        *unwrapBKey;
  guint8      *tokenID;
  guint8      *reqHMACkey;
  guint8      *respHMACkey;
  guint8      *respXORkey;
} FxACallbackData;
#endif

enum {
  COL_TITLE_ELIDED,
  COL_ENCODING,
  NUM_COLS
};

G_DEFINE_TYPE (PrefsDialog, prefs_dialog, GTK_TYPE_DIALOG)

#ifdef ENABLE_SYNC
static FxACallbackData *
fxa_callback_data_new (PrefsDialog *dialog,
                       const char  *email,
                       const char  *uid,
                       const char  *sessionToken,
                       const char  *keyFetchToken,
                       const char  *unwrapBKey,
                       guint8      *tokenID,
                       guint8      *reqHMACkey,
                       guint8      *respHMACkey,
                       guint8      *respXORkey)
{
  FxACallbackData *data = g_slice_new (FxACallbackData);

  data->dialog = g_object_ref (dialog);
  data->email = g_strdup (email);
  data->uid = g_strdup (uid);
  data->sessionToken = g_strdup (sessionToken);
  data->keyFetchToken = g_strdup (keyFetchToken);
  data->unwrapBKey = g_strdup (unwrapBKey);
  data->tokenID = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  memcpy (data->tokenID, tokenID, EPHY_SYNC_TOKEN_LENGTH);
  data->reqHMACkey = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  memcpy (data->reqHMACkey, reqHMACkey, EPHY_SYNC_TOKEN_LENGTH);
  data->respHMACkey = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  memcpy (data->respHMACkey, respHMACkey, EPHY_SYNC_TOKEN_LENGTH);
  data->respXORkey = g_malloc (2 * EPHY_SYNC_TOKEN_LENGTH);
  memcpy (data->respXORkey, respXORkey, 2 * EPHY_SYNC_TOKEN_LENGTH);

  return data;
}

static void
fxa_callback_data_free (FxACallbackData *data)
{
  g_assert (data != NULL);

  g_object_unref (data->dialog);
  g_free (data->email);
  g_free (data->uid);
  g_free (data->sessionToken);
  g_free (data->keyFetchToken);
  g_free (data->unwrapBKey);
  g_free (data->tokenID);
  g_free (data->reqHMACkey);
  g_free (data->respHMACkey);
  g_free (data->respXORkey);

  g_slice_free (FxACallbackData, data);
}
#endif

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

#ifdef ENABLE_SYNC
  if (dialog->fxa_web_view != NULL) {
    webkit_user_content_manager_unregister_script_message_handler (dialog->fxa_manager,
                                                                   "accountsCommandHandler");
    webkit_user_script_unref (dialog->fxa_script);
    g_object_unref (dialog->fxa_manager);
  }

  if (dialog->fxa_id != 0) {
    g_source_remove (dialog->fxa_id);
    dialog->fxa_id = 0;
  }
#endif

  G_OBJECT_CLASS (prefs_dialog_parent_class)->finalize (object);
}

#ifdef ENABLE_SYNC
static void
hide_fxa_iframe (PrefsDialog *dialog,
                 const char  *email)
{
  char *text;
  char *account;

  account = g_strdup_printf ("<b>%s</b>", email);
  /* Translators: the %s refers to the email of the currently logged in user. */
  text = g_strdup_printf (_("Currently logged in as %s"), account);
  gtk_label_set_markup (GTK_LABEL (dialog->sync_sign_out_details), text);

  gtk_container_remove (GTK_CONTAINER (dialog->sync_authenticate_box),
                        dialog->sync_sign_in_box);
  gtk_box_pack_start (GTK_BOX (dialog->sync_authenticate_box),
                      dialog->sync_sign_out_box,
                      TRUE, TRUE, 0);

  g_free (text);
  g_free (account);
}

static void
sync_tokens_store_finished_cb (EphySyncService *service,
                               GError          *error,
                               PrefsDialog     *dialog)
{
  g_assert (EPHY_IS_SYNC_SERVICE (service));
  g_assert (EPHY_IS_PREFS_DIALOG (dialog));

  if (error == NULL) {
    /* Show the 'Signed in' panel. */
    hide_fxa_iframe (dialog, ephy_sync_service_get_user_email (service));

    /* Do a first time sync and set a periodical sync to be executed. */
    ephy_sync_service_sync_bookmarks (service, TRUE);
    ephy_sync_service_start_periodical_sync (service, FALSE);
  } else {
    char *message;

    /* Destroy the current session. */
    ephy_sync_service_destroy_session (service, NULL);

    /* Unset the email and tokens. */
    g_settings_set_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_SYNC_USER, "");
    ephy_sync_service_clear_tokens (service);

    /* Display the error message to the user. */
    message = g_strdup_printf ("<span fgcolor='#e6780b'>%s</span>", error->message);
    gtk_label_set_markup (GTK_LABEL (dialog->sync_sign_in_details), message);
    gtk_widget_set_visible (dialog->sync_sign_in_details, TRUE);
    webkit_web_view_load_uri (dialog->fxa_web_view, FXA_IFRAME_URL);

    g_free (message);
  }
}

static gboolean
poll_fxa_server (gpointer user_data)
{
  FxACallbackData *data;
  EphySyncService *service;
  char *bundle;

  data = (FxACallbackData *)user_data;
  service = ephy_shell_get_sync_service (ephy_shell_get_default ());
  bundle = ephy_sync_service_start_sign_in (service, data->tokenID, data->reqHMACkey);

  if (bundle != NULL) {
    ephy_sync_service_finish_sign_in (service, data->email, data->uid,
                                      data->sessionToken, data->keyFetchToken,
                                      data->unwrapBKey, bundle,
                                      data->respHMACkey, data->respXORkey);

    g_free (bundle);
    fxa_callback_data_free (data);
    data->dialog->fxa_id = 0;

    return G_SOURCE_REMOVE;
  }

  return G_SOURCE_CONTINUE;
}

static void
inject_data_to_server (PrefsDialog *dialog,
                       const char  *type,
                       const char  *status,
                       const char  *data)
{
  char *json;
  char *script;

  if (data == NULL)
    json = g_strdup_printf ("{'type': '%s', 'content': {'status': '%s'}}",
                            type, status);
  else
    json = g_strdup_printf ("{'type': '%s', 'content': {'status': '%s', 'data': %s}}",
                            type, status, data);

  script = g_strdup_printf ("window.postMessage(%s, '%s');", json, FXA_IFRAME_URL);

  /* No callback, we don't expect any response from the server. */
  webkit_web_view_run_javascript (dialog->fxa_web_view,
                                  script,
                                  NULL, NULL, NULL);

  g_free (json);
  g_free (script);
}

static void
server_message_received_cb (WebKitUserContentManager *manager,
                            WebKitJavascriptResult   *result,
                            PrefsDialog              *dialog)
{
  EphySyncService *service;
  JsonParser *parser;
  JsonObject *object;
  JsonObject *detail;
  char *json_string;
  const char *type;
  const char *command;

  service = ephy_shell_get_sync_service (ephy_shell_get_default ());
  json_string = ephy_embed_utils_get_js_result_as_string (result);
  parser = json_parser_new ();
  json_parser_load_from_data (parser, json_string, -1, NULL);
  object = json_node_get_object (json_parser_get_root (parser));
  type = json_object_get_string_member (object, "type");

  /* The only message type we can receive is FirefoxAccountsCommand. */
  if (g_strcmp0 (type, "FirefoxAccountsCommand") != 0) {
    g_warning ("Unknown command type: %s", type);
    goto out;
  }

  detail = json_object_get_object_member (object, "detail");
  command = json_object_get_string_member (detail, "command");

  if (g_strcmp0 (command, "loaded") == 0) {
    LOG ("Loaded Firefox Sign In iframe");
    gtk_widget_set_visible (dialog->sync_sign_in_details, FALSE);
  } else if (g_strcmp0 (command, "can_link_account") == 0) {
    /* We need to confirm a relink. */
    inject_data_to_server (dialog, "message", "can_link_account", "{'ok': true}");
  } else if (g_strcmp0 (command, "login") == 0) {
    JsonObject *data = json_object_get_object_member (detail, "data");
    const char *email = json_object_get_string_member (data, "email");
    const char *uid = json_object_get_string_member (data, "uid");
    const char *sessionToken = json_object_get_string_member (data, "sessionToken");
    const char *keyFetchToken = json_object_get_string_member (data, "keyFetchToken");
    const char *unwrapBKey = json_object_get_string_member (data, "unwrapBKey");
    guint8 *tokenID;
    guint8 *reqHMACkey;
    guint8 *respHMACkey;
    guint8 *respXORkey;
    char *text;

    inject_data_to_server (dialog, "message", "login", NULL);

    /* Cannot retrieve the sync keys without keyFetchToken or unwrapBKey. */
    if (keyFetchToken == NULL || unwrapBKey == NULL) {
      g_warning ("Ignoring login with keyFetchToken or unwrapBKey missing!"
                 "Cannot retrieve sync keys with one of them missing.");
      ephy_sync_service_destroy_session (service, sessionToken);

      text = g_strdup_printf ("<span fgcolor='#e6780b'>%s</span>",
                              _("Something went wrong, please try again."));
      gtk_label_set_markup (GTK_LABEL (dialog->sync_sign_in_details), text);
      gtk_widget_set_visible (dialog->sync_sign_in_details, TRUE);
      webkit_web_view_load_uri (dialog->fxa_web_view, FXA_IFRAME_URL);

      g_free (text);
      goto out;
    }

    /* Derive tokenID, reqHMACkey, respHMACkey and respXORkey from the keyFetchToken.
     * tokenID and reqHMACkey are used to make a HAWK request to the "GET /account/keys"
     * API. The server looks up the stored table entry with tokenID, checks the request
     * HMAC for validity, then returns the pre-encrypted response.
     * See https://github.com/mozilla/fxa-auth-server/wiki/onepw-protocol#fetching-sync-keys */
    ephy_sync_crypto_process_key_fetch_token (keyFetchToken,
                                              &tokenID, &reqHMACkey,
                                              &respHMACkey, &respXORkey);

    /* If the account is not verified, then poll the server repeatedly
     * until the verification has finished. */
    if (json_object_get_boolean_member (data, "verified") == FALSE) {
      FxACallbackData *cb_data;

      text = g_strdup_printf ("<span fgcolor='#e6780b'>%s</span>",
                              _("Please don’t leave this page until you have completed the verification."));
      gtk_label_set_markup (GTK_LABEL (dialog->sync_sign_in_details), text);
      gtk_widget_set_visible (dialog->sync_sign_in_details, TRUE);

      cb_data = fxa_callback_data_new (dialog, email, uid, sessionToken,
                                       keyFetchToken, unwrapBKey, tokenID,
                                       reqHMACkey, respHMACkey, respXORkey);
      dialog->fxa_id = g_timeout_add_seconds (2, (GSourceFunc)poll_fxa_server, cb_data);

      g_free (text);
    } else {
      char *bundle;

      bundle = ephy_sync_service_start_sign_in (service, tokenID, reqHMACkey);
      ephy_sync_service_finish_sign_in (service, email, uid, sessionToken, keyFetchToken,
                                        unwrapBKey, bundle, respHMACkey, respXORkey);

      g_free (bundle);
    }
  } else if (g_strcmp0 (command, "session_status") == 0) {
    /* We are not signed in at this time, which we signal by returning an error. */
    inject_data_to_server (dialog, "message", "error", NULL);
  } else if (g_strcmp0 (command, "sign_out") == 0) {
    /* We are not signed in at this time. We should never get a sign out message! */
    inject_data_to_server (dialog, "message", "error", NULL);
  }

out:
  g_free (json_string);
  g_object_unref (parser);
}

static void
setup_fxa_sign_in_view (PrefsDialog *dialog)
{
  EphyEmbedShell *shell;
  WebKitWebContext *embed_context;
  WebKitWebContext *sync_context;
  const char *js = "\"use strict\";"
                   "function handleAccountsCommand(evt) {"
                   "  let j = {type: evt.type, detail: evt.detail};"
                   "  window.webkit.messageHandlers.accountsCommandHandler.postMessage(JSON.stringify(j));"
                   "};"
                   "window.addEventListener(\"FirefoxAccountsCommand\", handleAccountsCommand);";

  dialog->fxa_script = webkit_user_script_new (js,
                                               WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
                                               WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END,
                                               NULL, NULL);
  dialog->fxa_manager = webkit_user_content_manager_new ();
  webkit_user_content_manager_add_script (dialog->fxa_manager, dialog->fxa_script);
  g_signal_connect (dialog->fxa_manager,
                    "script-message-received::accountsCommandHandler",
                    G_CALLBACK (server_message_received_cb),
                    dialog);
  webkit_user_content_manager_register_script_message_handler (dialog->fxa_manager,
                                                               "accountsCommandHandler");

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
  g_object_unref (sync_context);

  gtk_widget_set_visible (GTK_WIDGET (dialog->fxa_web_view), TRUE);
  gtk_widget_set_size_request (GTK_WIDGET (dialog->fxa_web_view), 450, 450);
  webkit_web_view_load_uri (dialog->fxa_web_view, FXA_IFRAME_URL);

  gtk_widget_set_visible (dialog->sync_sign_in_details, FALSE);
  gtk_container_add (GTK_CONTAINER (dialog->sync_sign_in_box),
                     GTK_WIDGET (dialog->fxa_web_view));
}

static void
on_sync_sign_out_button_clicked (GtkWidget   *button,
                                 PrefsDialog *dialog)
{
  EphySyncService *service;

  service = ephy_shell_get_sync_service (ephy_shell_get_default ());

  /* Destroy session and delete tokens. */
  ephy_sync_service_stop_periodical_sync (service);
  ephy_sync_service_destroy_session (service, NULL);
  ephy_sync_service_clear_storage_credentials (service);
  ephy_sync_service_clear_tokens (service);
  ephy_sync_secret_forget_tokens ();
  ephy_sync_service_set_user_email (service, NULL);
  ephy_sync_service_set_sync_time (service, 0);

  g_settings_set_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_SYNC_USER, "");

  /* Show sign in box. */
  if (dialog->fxa_web_view == NULL)
    setup_fxa_sign_in_view (dialog);
  else
    webkit_web_view_load_uri (dialog->fxa_web_view, FXA_IFRAME_URL);

  gtk_container_remove (GTK_CONTAINER (dialog->sync_authenticate_box),
                        dialog->sync_sign_out_box);
  gtk_box_pack_start (GTK_BOX (dialog->sync_authenticate_box),
                      dialog->sync_sign_in_box,
                      TRUE, TRUE, 0);
  gtk_widget_set_visible (dialog->sync_sign_in_details, FALSE);
}
#endif

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

  passwords_dialog = ephy_passwords_dialog_new ();

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

#ifdef ENABLE_SYNC
  /* sync */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_authenticate_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_sign_in_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_sign_in_details);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_sign_out_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_sign_out_details);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_sign_out_button);
#endif
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, notebook);

  gtk_widget_class_bind_template_callback (widget_class, on_manage_cookies_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_manage_passwords_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_search_engine_dialog_button_clicked);
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

static gboolean
combo_get_mapping (GValue   *value,
                   GVariant *variant,
                   gpointer  user_data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean valid = FALSE;
  const char *settings_name;
  int i;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (user_data));
  valid = gtk_tree_model_get_iter_first (model, &iter);
  settings_name = g_variant_get_string (variant, NULL);
  i = 0;

  while (valid) {
    char *item_name;
    gtk_tree_model_get (model, &iter, 1, &item_name, -1);

    if (g_strcmp0 (item_name, settings_name) == 0) {
      g_value_set_int (value, i);
      g_free (item_name);
      break;
    }

    i++;
    valid = gtk_tree_model_iter_next (model, &iter);
    g_free (item_name);
  }

  return TRUE;
}

static GVariant *
combo_set_mapping (const GValue       *value,
                   const GVariantType *expected_type,
                   gpointer            user_data)
{
  GVariant *variant = NULL;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean valid = FALSE;
  int n;

  n = g_value_get_int (value);
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (user_data));
  valid = gtk_tree_model_iter_nth_child (model, &iter, NULL, n);

  if (valid) {
    char *item_name;
    gtk_tree_model_get (model, &iter, 1, &item_name, -1);

    variant = g_variant_new_string (item_name);

    g_free (item_name);
  }

  return variant;
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
  EphyFileChooser *fc;
  char *dir;

  dir = ephy_file_get_downloads_dir ();

  fc = ephy_file_chooser_new (_("Select a Directory"),
                              GTK_WIDGET (dialog),
                              GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                              EPHY_FILE_FILTER_NONE);

  /* Unset the destroy-with-parent, since gtkfilechooserbutton doesn't
   * expect this */
  gtk_window_set_destroy_with_parent (GTK_WINDOW (fc), FALSE);

  button = gtk_file_chooser_button_new_with_dialog (GTK_WIDGET (fc));
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

#ifdef ENABLE_SYNC
static void
setup_sync_page (PrefsDialog *dialog)
{
  EphySyncService *service;
  char *account;
  char *text;

  service = ephy_shell_get_sync_service (ephy_shell_get_default ());

  if (ephy_sync_service_is_signed_in (service) == FALSE) {
    setup_fxa_sign_in_view (dialog);
    gtk_container_remove (GTK_CONTAINER (dialog->sync_authenticate_box),
                          dialog->sync_sign_out_box);
  } else {
    gtk_container_remove (GTK_CONTAINER (dialog->sync_authenticate_box),
                          dialog->sync_sign_in_box);

    account = g_strdup_printf ("<b>%s</b>", ephy_sync_service_get_user_email (service));
    /* Translators: the %s refers to the email of the currently logged in user. */
    text = g_strdup_printf (_("Currently logged in as %s"), account);
    gtk_label_set_markup (GTK_LABEL (dialog->sync_sign_out_details), text);

    g_free (text);
    g_free (account);
  }

  g_signal_connect_object (service, "sync-tokens-store-finished",
                           G_CALLBACK (sync_tokens_store_finished_cb),
                           dialog, 0);
}
#endif

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
#ifdef ENABLE_SYNC
  if (mode != EPHY_EMBED_SHELL_MODE_APPLICATION) {
    setup_sync_page (dialog);

    /* TODO: Switch back to using a template callback in class_init once sync is unconditionally enabled. */
    g_signal_connect (dialog->sync_sign_out_button, "clicked",
                      G_CALLBACK (on_sync_sign_out_button_clicked), dialog);
  } else
    gtk_notebook_remove_page (GTK_NOTEBOOK (dialog->notebook), -1);
#else
  gtk_notebook_remove_page (GTK_NOTEBOOK (dialog->notebook), -1);
#endif

  ephy_gui_ensure_window_group (GTK_WINDOW (dialog));
  g_signal_connect (dialog, "response",
                    G_CALLBACK (prefs_dialog_response_cb), dialog);
}
