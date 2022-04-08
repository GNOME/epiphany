/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2008, 2009 Gustavo Noronha Silva
 *  Copyright © 2009, 2010, 2014 Igalia S.L.
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
#include "ephy-web-view.h"

#include "ephy-about-handler.h"
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-embed-utils.h"
#include "ephy-embed.h"
#include "ephy-favicon-helpers.h"
#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "ephy-file-monitor.h"
#include "ephy-filters-manager.h"
#include "ephy-gsb-utils.h"
#include "ephy-history-service.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-output-encoding.h"
#include "ephy-permissions-manager.h"
#include "ephy-prefs.h"
#include "ephy-reader-handler.h"
#include "ephy-settings.h"
#include "ephy-snapshot-service.h"
#include "ephy-string.h"
#include "ephy-uri-helpers.h"
#include "ephy-pdf-handler.h"
#include "ephy-view-source-handler.h"
#include "ephy-web-app-utils.h"
#include "ephy-zoom.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <handy.h>

/**
 * SECTION:ephy-web-view
 * @short_description: Epiphany custom #WebkitWebView
 *
 * #EphyWebView wraps #WebkitWebView implementing custom functionality on top of
 * it.
 */

#define EPHY_PAGE_TEMPLATE_ERROR         "/org/gnome/epiphany/page-templates/error.html"
#define EPHY_PAGE_TEMPLATE_ERROR_CSS     "/org/gnome/epiphany/page-templates/error.css"

static guint64 web_view_uid = 1;

struct _EphyWebView {
  WebKitWebView parent_instance;

  EphySecurityLevel security_level;
  EphyWebViewDocumentType document_type;
  EphyWebViewNavigationFlags nav_flags;

  /* Flags */
  guint is_blank : 1;
  guint is_setting_zoom : 1;
  guint load_failed : 1;
  guint history_frozen : 1;
  guint ever_committed : 1;
  guint in_auth_dialog : 1;

  char *address;
  char *display_address;
  char *typed_address;
  char *last_committed_address;
  char *loading_message;
  char *link_message;
  GdkPixbuf *icon;

  /* Reader mode */
  gboolean entering_reader_mode;
  gboolean reader_mode_available;
  guint reader_js_timeout;

  /* Local file watch. */
  EphyFileMonitor *file_monitor;

  GtkWidget *geolocation_info_bar;
  GtkWidget *notification_info_bar;
  GtkWidget *microphone_info_bar;
  GtkWidget *webcam_info_bar;
  GtkWidget *webcam_mic_info_bar;
  GtkWidget *password_info_bar;
  GtkWidget *password_form_info_bar;
  GtkWidget *itp_info_bar;

  EphyHistoryService *history_service;

  GCancellable *cancellable;

  guint snapshot_timeout_id;
  char *pending_snapshot_uri;

  EphyHistoryPageVisitType visit_type;

  /* TLS information. */
  GTlsCertificate *certificate;
  GTlsCertificateFlags tls_errors;

  gboolean bypass_safe_browsing;
  gboolean loading_error_page;
  char *tls_error_failing_uri;

  EphyWebViewErrorPage error_page;

  guint unresponsive_process_timeout_id;
  GtkWidget *unresponsive_process_dialog;

  guint64 uid;
};

enum {
  PROP_0,
  PROP_ADDRESS,
  PROP_DOCUMENT_TYPE,
  PROP_ICON,
  PROP_LINK_MESSAGE,
  PROP_NAVIGATION,
  PROP_SECURITY,
  PROP_STATUS_MESSAGE,
  PROP_TYPED_ADDRESS,
  PROP_IS_BLANK,
  PROP_READER_MODE,
  PROP_DISPLAY_ADDRESS,
  PROP_ENTERING_READER_MODE,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

G_DEFINE_TYPE (EphyWebView, ephy_web_view, WEBKIT_TYPE_WEB_VIEW)

static void
open_response_cb (GtkFileChooser           *dialog,
                  gint                      response,
                  WebKitFileChooserRequest *request)
{
  if (response == GTK_RESPONSE_ACCEPT) {
    GSList *file_list = gtk_file_chooser_get_filenames (dialog);
    GPtrArray *file_array = g_ptr_array_new ();
    g_autoptr (GFile) current_folder = NULL;
    g_autofree char *current_folder_path = NULL;

    for (GSList *file = file_list; file; file = g_slist_next (file))
      g_ptr_array_add (file_array, file->data);

    g_ptr_array_add (file_array, NULL);
    webkit_file_chooser_request_select_files (request, (const char * const *)file_array->pdata);
    g_slist_free_full (file_list, g_free);
    g_ptr_array_free (file_array, TRUE);

    current_folder = gtk_file_chooser_get_current_folder_file (dialog);
    if (current_folder) {
      current_folder_path = g_file_get_path (current_folder);
      g_settings_set_string (EPHY_SETTINGS_WEB,
                             EPHY_PREFS_WEB_LAST_UPLOAD_DIRECTORY,
                             current_folder_path);
    }
  } else {
    webkit_file_chooser_request_cancel (request);
  }

  g_object_unref (request);
  g_object_unref (dialog);
}

static gboolean
ephy_web_view_run_file_chooser (WebKitWebView            *web_view,
                                WebKitFileChooserRequest *request)
{
  GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
  GtkFileChooser *dialog;
  gboolean allows_multiple_selection = webkit_file_chooser_request_get_select_multiple (request);
  GtkFileFilter *filter = webkit_file_chooser_request_get_mime_types_filter (request);
  g_autofree char *last_directory_path = NULL;

  dialog = ephy_create_file_chooser (_("Open"),
                                     GTK_WIDGET (toplevel),
                                     GTK_FILE_CHOOSER_ACTION_OPEN,
                                     EPHY_FILE_FILTER_ALL);

  if (filter) {
    gtk_file_chooser_add_filter (dialog, filter);
    gtk_file_chooser_set_filter (dialog, filter);
  }

  last_directory_path = g_settings_get_string (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_LAST_UPLOAD_DIRECTORY);

  if (last_directory_path && last_directory_path[0]) {
    g_autoptr (GFile) last_directory = NULL;
    g_autoptr (GError) error = NULL;

    last_directory = g_file_new_for_path (last_directory_path);
    gtk_file_chooser_set_current_folder_file (dialog, last_directory, &error);

    if (error)
      g_warning ("Failed to set current folder %s: %s", last_directory_path, error->message);
  }

  gtk_file_chooser_set_select_multiple (dialog, allows_multiple_selection);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (open_response_cb),
                    g_object_ref (request));

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));

  return TRUE;
}

static void
ephy_web_view_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  EphyWebView *view = EPHY_WEB_VIEW (object);

  switch (prop_id) {
    case PROP_ADDRESS:
      g_value_set_string (value, view->address);
      break;
    case PROP_TYPED_ADDRESS:
      g_value_set_string (value, view->typed_address);
      break;
    case PROP_DOCUMENT_TYPE:
      g_value_set_enum (value, view->document_type);
      break;
    case PROP_ICON:
      g_value_set_object (value, view->icon);
      break;
    case PROP_LINK_MESSAGE:
      g_value_set_string (value, view->link_message);
      break;
    case PROP_NAVIGATION:
      g_value_set_flags (value, view->nav_flags);
      break;
    case PROP_SECURITY:
      g_value_set_enum (value, view->security_level);
      break;
    case PROP_STATUS_MESSAGE:
      g_value_set_string (value, ephy_web_view_get_status_message (EPHY_WEB_VIEW (object)));
      break;
    case PROP_IS_BLANK:
      g_value_set_boolean (value, view->is_blank);
      break;
    case PROP_READER_MODE:
      g_value_set_boolean (value, view->reader_mode_available);
      break;
    case PROP_DISPLAY_ADDRESS:
      g_value_set_string (value, view->display_address);
      break;
    case PROP_ENTERING_READER_MODE:
      g_value_set_boolean (value, view->entering_reader_mode);
      break;
    default:
      break;
  }
}

static void
ephy_web_view_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  switch (prop_id) {
    case PROP_TYPED_ADDRESS:
      ephy_web_view_set_typed_address (EPHY_WEB_VIEW (object), g_value_get_string (value));
      break;
    case PROP_ADDRESS:
    case PROP_DOCUMENT_TYPE:
    case PROP_ICON:
    case PROP_LINK_MESSAGE:
    case PROP_NAVIGATION:
    case PROP_SECURITY:
    case PROP_STATUS_MESSAGE:
    case PROP_IS_BLANK:
    case PROP_READER_MODE:
    case PROP_DISPLAY_ADDRESS:
    case PROP_ENTERING_READER_MODE:
      /* read only */
      break;
    default:
      break;
  }
}

static gboolean
ephy_web_view_button_press_event (GtkWidget      *widget,
                                  GdkEventButton *event)
{
  /* These are the special cases WebkitWebView doesn't handle but we have an
   * interest in handling. */

  /* Handle typical back/forward mouse buttons. */
  if (event->button == 8) {
    webkit_web_view_go_back (WEBKIT_WEB_VIEW (widget));
    return TRUE;
  }

  if (event->button == 9) {
    webkit_web_view_go_forward (WEBKIT_WEB_VIEW (widget));
    return TRUE;
  }

  /* Let WebKitWebView handle this. */
  return GTK_WIDGET_CLASS (ephy_web_view_parent_class)->button_press_event (widget, event);
}

static void
untrack_info_bar (GtkWidget **tracked_info_bar)
{
  g_assert (tracked_info_bar);
  g_assert (!*tracked_info_bar || GTK_IS_INFO_BAR (*tracked_info_bar));

  if (*tracked_info_bar) {
    g_object_remove_weak_pointer (G_OBJECT (*tracked_info_bar), (gpointer *)tracked_info_bar);
    gtk_widget_destroy (*tracked_info_bar);
    *tracked_info_bar = NULL;
  }
}

static void
track_info_bar (GtkWidget  *new_info_bar,
                GtkWidget **tracked_info_bar)
{
  g_assert (GTK_IS_INFO_BAR (new_info_bar));
  g_assert (tracked_info_bar);
  g_assert (!*tracked_info_bar || GTK_IS_INFO_BAR (*tracked_info_bar));

  untrack_info_bar (tracked_info_bar);

  *tracked_info_bar = new_info_bar;
  g_object_add_weak_pointer (G_OBJECT (new_info_bar),
                             (gpointer *)tracked_info_bar);
}

static GtkWidget *
ephy_web_view_create_form_auth_save_confirmation_info_bar (EphyWebView *web_view,
                                                           const char  *origin,
                                                           const char  *username)
{
  GtkWidget *info_bar;
  GtkWidget *content_area;
  GtkWidget *label;
  char *message;

  LOG ("Going to show infobar about %s", webkit_web_view_get_uri (WEBKIT_WEB_VIEW (web_view)));

  info_bar = gtk_info_bar_new_with_buttons (_("Not No_w"), GTK_RESPONSE_CLOSE,
                                            _("_Never Save"), GTK_RESPONSE_REJECT,
                                            _("_Save"), GTK_RESPONSE_YES,
                                            NULL);

  label = gtk_label_new (NULL);
  /* Translators: The %s the hostname where this is happening.
   * Example: mail.google.com.
   */
  message = g_markup_printf_escaped (_("Do you want to save your password for “%s”?"), origin);
  gtk_label_set_markup (GTK_LABEL (label), message);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  g_free (message);

  content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (info_bar));
  gtk_container_add (GTK_CONTAINER (content_area), label);
  gtk_widget_show (label);

  track_info_bar (info_bar, &web_view->password_info_bar);

  ephy_embed_add_top_widget (EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (web_view),
                             info_bar,
                             EPHY_EMBED_TOP_WIDGET_POLICY_RETAIN_ON_TRANSITION);

  return info_bar;
}

typedef struct {
  EphyPasswordSaveRequestCallback callback;
  gpointer callback_data;
  GDestroyNotify callback_destroy;
} SaveRequestData;

static void
save_auth_request_destroy (SaveRequestData *data,
                           GClosure        *ignored)
{
  if (data->callback_destroy)
    data->callback_destroy (data->callback_data);

  g_free (data);
}

static void
info_bar_save_request_response_cb (GtkInfoBar      *info_bar,
                                   gint             response_id,
                                   SaveRequestData *data)
{
  g_assert (data->callback);
  data->callback (response_id, data->callback_data);
  gtk_widget_destroy (GTK_WIDGET (info_bar));
}

void
ephy_web_view_show_auth_form_save_request (EphyWebView                     *web_view,
                                           const char                      *origin,
                                           const char                      *username,
                                           EphyPasswordSaveRequestCallback  response_callback,
                                           gpointer                         response_data,
                                           GDestroyNotify                   response_destroy)
{
  GtkWidget *info_bar;
  SaveRequestData *data;

  info_bar = ephy_web_view_create_form_auth_save_confirmation_info_bar (web_view, origin, username);

  data = g_new (SaveRequestData, 1);
  data->callback = response_callback;
  data->callback_data = response_data;
  data->callback_destroy = response_destroy;

  g_signal_connect_data (info_bar, "response",
                         G_CALLBACK (info_bar_save_request_response_cb),
                         data, (GClosureNotify)save_auth_request_destroy, 0);

  gtk_widget_show (info_bar);
}

static void
update_navigation_flags (WebKitWebView *view)
{
  guint flags = 0;

  if (webkit_web_view_can_go_back (view))
    flags |= EPHY_WEB_VIEW_NAV_BACK;

  if (webkit_web_view_can_go_forward (view))
    flags |= EPHY_WEB_VIEW_NAV_FORWARD;

  if (EPHY_WEB_VIEW (view)->nav_flags != (EphyWebViewNavigationFlags)flags) {
    EPHY_WEB_VIEW (view)->nav_flags = (EphyWebViewNavigationFlags)flags;

    g_object_notify_by_pspec (G_OBJECT (view), obj_properties[PROP_NAVIGATION]);
  }
}

static void
ephy_web_view_freeze_history (EphyWebView *view)
{
  view->history_frozen = TRUE;
}

static void
ephy_web_view_thaw_history (EphyWebView *view)
{
  view->history_frozen = FALSE;
}

static gboolean
ephy_web_view_is_history_frozen (EphyWebView *view)
{
  return view->history_frozen;
}

static void
got_snapshot_path_cb (EphySnapshotService *service,
                      GAsyncResult        *result,
                      char                *url)
{
  char *snapshot;
  GError *error = NULL;

  snapshot = ephy_snapshot_service_get_snapshot_path_finish (service, result, &error);
  if (snapshot) {
    ephy_embed_shell_set_thumbnail_path (ephy_embed_shell_get_default (), url, snapshot);
    g_free (snapshot);
  } else {
    /* Bad luck, not something to warn about. */
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_info ("Failed to get snapshot for URL %s: %s", url, error->message);
    g_error_free (error);
  }
  g_free (url);
}

static void
take_snapshot (EphyWebView *view)
{
  EphySnapshotService *service = ephy_snapshot_service_get_default ();

  ephy_snapshot_service_get_snapshot_path_async (service, WEBKIT_WEB_VIEW (view),
                                                 view->cancellable,
                                                 (GAsyncReadyCallback)got_snapshot_path_cb,
                                                 g_strdup (view->pending_snapshot_uri));
}

static void
history_service_query_urls_cb (EphyHistoryService *service,
                               gboolean            success,
                               GList              *urls,
                               EphyWebView        *view)
{
  const char *url = webkit_web_view_get_uri (WEBKIT_WEB_VIEW (view));

  if (!success)
    goto out;

  /* Have we already started a new load? */
  if (g_strcmp0 (url, view->pending_snapshot_uri) != 0)
    goto out;

  for (GList *l = urls; l; l = g_list_next (l)) {
    EphyHistoryURL *history_url = l->data;

    /* Take snapshot if this URL is one of the top history results. */
    if (strcmp (history_url->url, view->pending_snapshot_uri) == 0) {
      take_snapshot (view);
      break;
    }
  }

out:
  g_clear_pointer (&view->pending_snapshot_uri, g_free);
  g_object_unref (view);
}

static gboolean
maybe_take_snapshot (EphyWebView *view)
{
  EphyEmbedShell *shell;
  EphyHistoryService *service;
  EphyHistoryQuery *query;

  view->snapshot_timeout_id = 0;

  if (view->error_page != EPHY_WEB_VIEW_ERROR_PAGE_NONE)
    return FALSE;

  shell = ephy_embed_shell_get_default ();
  service = ephy_embed_shell_get_global_history_service (shell);

  /* We want to save snapshots for just a couple more pages than are present
   * in the overview, so new snapshots are immediately available when the user
   * deletes a couple pages from the overview. Let's say five more.
   */
  query = ephy_history_query_new_for_overview ();
  query->limit += 5;
  ephy_history_service_query_urls (service, query, NULL,
                                   (EphyHistoryJobCallback)history_service_query_urls_cb,
                                   g_object_ref (view));
  ephy_history_query_free (query);

  return FALSE;
}

static void
_ephy_web_view_update_icon (EphyWebView *view)
{
  g_clear_object (&view->icon);

  if (view->address) {
    cairo_surface_t *icon_surface = webkit_web_view_get_favicon (WEBKIT_WEB_VIEW (view));
    if (icon_surface) {
      gint scale = gtk_widget_get_scale_factor (GTK_WIDGET (view));
      view->icon = ephy_pixbuf_get_from_surface_scaled (icon_surface, scale * FAVICON_SIZE, scale * FAVICON_SIZE);
    }
  }

  g_object_notify_by_pspec (G_OBJECT (view), obj_properties[PROP_ICON]);
}

static void
icon_changed_cb (EphyWebView *view,
                 GParamSpec  *pspec,
                 gpointer     user_data)
{
  _ephy_web_view_update_icon (view);
}

static void
password_form_focused_cb (EphyEmbedShell *shell,
                          guint64         page_id,
                          gboolean        insecure_form_action,
                          EphyWebView    *web_view)
{
  GtkWidget *info_bar;
  GtkWidget *label;
  GtkWidget *content_area;

  if (web_view->password_form_info_bar)
    return;
  if (webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (web_view)) != page_id)
    return;
  if (!insecure_form_action && ephy_security_level_is_secure (web_view->security_level))
    return;

  /* Translators: Message appears when insecure password form is focused. */
  label = gtk_label_new (_("Heads-up: this form is not secure. If you type your password, it will not be kept private."));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_widget_show (label);

  info_bar = gtk_info_bar_new ();
  gtk_info_bar_set_message_type (GTK_INFO_BAR (info_bar), GTK_MESSAGE_WARNING);
  gtk_info_bar_set_show_close_button (GTK_INFO_BAR (info_bar), TRUE);
  content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (info_bar));
  gtk_container_add (GTK_CONTAINER (content_area), label);

  g_signal_connect (info_bar, "response", G_CALLBACK (gtk_widget_hide), NULL);

  track_info_bar (info_bar, &web_view->password_form_info_bar);

  ephy_embed_add_top_widget (EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (web_view),
                             info_bar,
                             EPHY_EMBED_TOP_WIDGET_POLICY_DESTROY_ON_TRANSITION);
  gtk_widget_show (info_bar);
}

static void
allow_tls_certificate_cb (EphyEmbedShell *shell,
                          guint64         page_id,
                          EphyWebView    *view)
{
  g_autoptr (GUri) uri = NULL;

  if (webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)) != page_id)
    return;

  g_assert (G_IS_TLS_CERTIFICATE (view->certificate));
  g_assert (view->tls_error_failing_uri != NULL);

  uri = g_uri_parse (view->tls_error_failing_uri, G_URI_FLAGS_NONE, NULL);
  webkit_web_context_allow_tls_certificate_for_host (ephy_embed_shell_get_web_context (shell),
                                                     view->certificate,
                                                     g_uri_get_host (uri));
  ephy_web_view_load_url (view, ephy_web_view_get_address (view));
}

static void
allow_unsafe_browsing_cb (EphyEmbedShell *shell,
                          guint64         page_id,
                          EphyWebView    *view)
{
  if (webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)) != page_id)
    return;

  ephy_web_view_set_should_bypass_safe_browsing (view, TRUE);
  ephy_web_view_load_url (view, ephy_web_view_get_address (view));
}

static void
reload_page_cb (EphyEmbedShell *shell,
                guint64         page_id,
                EphyWebView    *view)
{
  if (webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)) != page_id)
    return;

  webkit_web_view_reload (WEBKIT_WEB_VIEW (view));
}

static void
_ephy_web_view_set_is_blank (EphyWebView *view,
                             gboolean     is_blank)
{
  if (view->is_blank != is_blank) {
    view->is_blank = is_blank;
    g_object_notify_by_pspec (G_OBJECT (view), obj_properties[PROP_IS_BLANK]);
  }
}

static void
readability_js_finish_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  EphyWebView *view = EPHY_WEB_VIEW (user_data);
  g_autoptr (WebKitJavascriptResult) js_result = NULL;
  g_autoptr (GError) error = NULL;
  JSCValue *jsc_value;

  js_result = webkit_web_view_run_javascript_finish (WEBKIT_WEB_VIEW (object), result, &error);
  if (!js_result) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Error running javascript: %s", error->message);
    return;
  }

  jsc_value = webkit_javascript_result_get_js_value (js_result);
  if (!jsc_value_is_boolean (jsc_value)) {
    return;
  }

  view->reader_mode_available = jsc_value_to_boolean (jsc_value);

  g_object_notify_by_pspec (G_OBJECT (view), obj_properties[PROP_READER_MODE]);
}

static gboolean
run_readability_js_if_needed (gpointer data)
{
  EphyWebView *web_view = data;

  /* Internal pages should never receive reader mode. */
  if (!ephy_embed_utils_is_no_show_address (web_view->address)) {
    webkit_web_view_run_javascript_from_gresource (WEBKIT_WEB_VIEW (web_view),
                                                   "/org/gnome/epiphany/readability/Readability-readerable.js",
                                                   web_view->cancellable,
                                                   readability_js_finish_cb,
                                                   web_view);
  }

  web_view->reader_js_timeout = 0;
  return G_SOURCE_REMOVE;
}

static void
title_changed_cb (WebKitWebView *web_view,
                  GParamSpec    *spec,
                  gpointer       data)
{
  const char *uri;
  const char *title;
  char *title_from_address = NULL;
  EphyWebView *webview = EPHY_WEB_VIEW (web_view);
  EphyHistoryService *history = webview->history_service;

  uri = webkit_web_view_get_uri (web_view);
  title = webkit_web_view_get_title (web_view);

  if (!title && uri)
    title = title_from_address = ephy_embed_utils_get_title_from_address (uri);

  /* FIXME: we don't follow the same rules for transforming uri here that we do
   * when adding it to the history db in load_committed. Should probably try to
   * use EphyWebView:address instead?
   */
  if (uri && title && *title && !ephy_web_view_is_history_frozen (webview))
    ephy_history_service_set_url_title (history, uri, title, NULL, NULL, NULL);

  g_free (title_from_address);
}

static void
ephy_web_view_set_display_address (EphyWebView *view)
{
  g_clear_pointer (&view->display_address, g_free);

  if (view->address) {
    if (g_str_has_prefix (view->address, EPHY_PDF_SCHEME))
      view->display_address = ephy_uri_decode (view->address + strlen (EPHY_PDF_SCHEME) + 1);
    else
      view->display_address = ephy_uri_decode (view->address);
  }
}

/*
 * Sets the view location to be address. Note that this function might
 * also set the typed-address property to NULL.
 */
static void
ephy_web_view_set_address (EphyWebView *view,
                           const char  *address)
{
  GObject *object = G_OBJECT (view);
  gboolean was_empty;

  if (g_strcmp0 (view->address, address) == 0)
    return;

  was_empty = view->address == NULL;
  g_free (view->address);
  view->address = g_strdup (address);

  ephy_web_view_set_display_address (view);

  _ephy_web_view_set_is_blank (view, ephy_embed_utils_url_is_empty (address));

  /* If the view was empty there is no need to clean the typed address. */
  if (!was_empty && ephy_web_view_is_loading (view) && view->typed_address != NULL)
    ephy_web_view_set_typed_address (view, NULL);

  g_object_notify_by_pspec (object, obj_properties[PROP_ADDRESS]);
  g_object_notify_by_pspec (object, obj_properties[PROP_DISPLAY_ADDRESS]);
}

static void
uri_changed_cb (WebKitWebView *web_view,
                GParamSpec    *spec,
                gpointer       data)
{
  EphyWebView *view = EPHY_WEB_VIEW (web_view);

  if (view->document_type != EPHY_WEB_VIEW_DOCUMENT_PDF)
    ephy_web_view_set_address (view,
                               webkit_web_view_get_uri (web_view));
}

static void
mouse_target_changed_cb (EphyWebView         *web_view,
                         WebKitHitTestResult *hit_test_result,
                         guint                modifiers,
                         gpointer             data)
{
  const char *message = NULL;

  if (webkit_hit_test_result_context_is_link (hit_test_result))
    message = webkit_hit_test_result_get_link_uri (hit_test_result);

  ephy_web_view_set_link_message (web_view, message);
}

static void
process_terminated_cb (EphyWebView                       *web_view,
                       WebKitWebProcessTerminationReason  reason,
                       gpointer                           user_data)
{
  EphyWebViewErrorPage error_page = EPHY_WEB_VIEW_ERROR_PROCESS_CRASH;

  switch (reason) {
    case WEBKIT_WEB_PROCESS_CRASHED:
      g_warning (_("Web process crashed"));
      break;
    case WEBKIT_WEB_PROCESS_EXCEEDED_MEMORY_LIMIT:
      g_warning (_("Web process terminated due to exceeding memory limit"));
      break;
    case WEBKIT_WEB_PROCESS_TERMINATED_BY_API:
      g_warning (_("Web process terminated by API request"));
      error_page = EPHY_WEB_VIEW_ERROR_UNRESPONSIVE_PROCESS;
      break;
  }

  if (!ephy_embed_has_load_pending (EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (web_view))) {
    ephy_web_view_load_error_page (web_view, ephy_web_view_get_address (web_view),
                                   error_page, NULL, NULL);
  }
}

static gboolean
unresponsive_process_timeout_cb (gpointer user_data);

static void
on_unresponsive_dialog_response (GtkDialog *dialog,
                                 int        response_id,
                                 gpointer   user_data)
{
  EphyWebView *web_view = EPHY_WEB_VIEW (user_data);

  if (response_id == GTK_RESPONSE_YES)
    webkit_web_view_terminate_web_process (WEBKIT_WEB_VIEW (web_view));
  else
    web_view->unresponsive_process_timeout_id = g_timeout_add_seconds_full (G_PRIORITY_HIGH,
                                                                            5,
                                                                            (GSourceFunc)unresponsive_process_timeout_cb,
                                                                            web_view,
                                                                            NULL);
  g_clear_pointer (&web_view->unresponsive_process_dialog, gtk_widget_destroy);
}

static gboolean
unresponsive_process_timeout_cb (gpointer user_data)
{
  EphyWebView *web_view = EPHY_WEB_VIEW (user_data);

  if (!gtk_widget_get_mapped (GTK_WIDGET (web_view)))
    return G_SOURCE_CONTINUE;

  web_view->unresponsive_process_dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (web_view))),
                                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
                                                                  GTK_MESSAGE_QUESTION,
                                                                  GTK_BUTTONS_NONE,
                                                                  _("The current page '%s' is unresponsive"),
                                                                  ephy_web_view_get_address (web_view));

  gtk_dialog_add_button (GTK_DIALOG (web_view->unresponsive_process_dialog), _("_Wait"), GTK_RESPONSE_NO);
  gtk_dialog_add_button (GTK_DIALOG (web_view->unresponsive_process_dialog), _("_Kill"), GTK_RESPONSE_YES);

  g_signal_connect (web_view->unresponsive_process_dialog, "response", G_CALLBACK (on_unresponsive_dialog_response), web_view);
  gtk_widget_show_all (web_view->unresponsive_process_dialog);

  web_view->unresponsive_process_timeout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
is_web_process_responsive_changed_cb (EphyWebView *web_view,
                                      GParamSpec  *pspec,
                                      gpointer     user_data)
{
  gboolean responsive = webkit_web_view_get_is_web_process_responsive (WEBKIT_WEB_VIEW (web_view));

  g_clear_handle_id (&web_view->unresponsive_process_timeout_id, g_source_remove);

  if (web_view->unresponsive_process_dialog && responsive) {
    g_signal_handlers_disconnect_by_func (web_view->unresponsive_process_dialog, on_unresponsive_dialog_response, web_view);
    g_clear_pointer (&web_view->unresponsive_process_dialog, gtk_widget_destroy);
  }

  if (!responsive) {
    web_view->unresponsive_process_timeout_id = g_timeout_add_seconds_full (G_PRIORITY_HIGH,
                                                                            10,
                                                                            (GSourceFunc)unresponsive_process_timeout_cb,
                                                                            web_view,
                                                                            NULL);
  }
}

static gboolean
decide_policy_cb (WebKitWebView            *web_view,
                  WebKitPolicyDecision     *decision,
                  WebKitPolicyDecisionType  decision_type,
                  gpointer                  user_data)
{
  WebKitResponsePolicyDecision *response_decision;
  WebKitURIResponse *response;
  WebKitURIRequest *request;
  WebKitWebResource *main_resource;
  EphyWebViewDocumentType type;
  const char *mime_type;
  const char *request_uri;
  gboolean is_main_resource;

  if (decision_type != WEBKIT_POLICY_DECISION_TYPE_RESPONSE)
    return FALSE;

  /* If WebKit can handle the MIME type, let it.
   * Otherwise, we'll start a download.
   */
  response_decision = WEBKIT_RESPONSE_POLICY_DECISION (decision);
  if (webkit_response_policy_decision_is_mime_type_supported (response_decision))
    return FALSE;

  response = webkit_response_policy_decision_get_response (response_decision);
  mime_type = webkit_uri_response_get_mime_type (response);
  request = webkit_response_policy_decision_get_request (response_decision);
  request_uri = webkit_uri_request_get_uri (request);

  main_resource = webkit_web_view_get_main_resource (web_view);
  is_main_resource = g_strcmp0 (webkit_web_resource_get_uri (main_resource), request_uri) == 0;

  /* If it's not the main resource, we don't need to set the document type. */
  if (is_main_resource) {
    const char *method = webkit_uri_request_get_http_method (request);

    type = EPHY_WEB_VIEW_DOCUMENT_OTHER;
    if (strcmp (mime_type, "text/html") == 0 || strcmp (mime_type, "text/plain") == 0) {
      type = EPHY_WEB_VIEW_DOCUMENT_HTML;
    } else if (strcmp (mime_type, "application/xhtml+xml") == 0) {
      type = EPHY_WEB_VIEW_DOCUMENT_XML;
    } else if (strncmp (mime_type, "image/", 6) == 0) {
      type = EPHY_WEB_VIEW_DOCUMENT_IMAGE;
    } else if (strcmp (mime_type, "application/pdf") == 0 && (!method || strcmp (method, "GET") == 0)) {
      g_autofree char *pdf_uri = NULL;

      /* FIXME: figure out how to make PDFs work in iframes. */
      type = EPHY_WEB_VIEW_DOCUMENT_PDF;
      EPHY_WEB_VIEW (web_view)->document_type = type;

      pdf_uri = g_strconcat (EPHY_PDF_SCHEME, ":", request_uri, NULL);

      webkit_web_view_load_uri (web_view, pdf_uri);

      return FALSE;
    }

    /* FIXME: maybe it makes more sense to have an API to query the mime
     * type when the load of a page starts than doing this here.
     */
    if (EPHY_WEB_VIEW (web_view)->document_type != type) {
      EPHY_WEB_VIEW (web_view)->document_type = type;

      g_object_notify_by_pspec (G_OBJECT (web_view), obj_properties[PROP_DOCUMENT_TYPE]);
    }
  }

  webkit_policy_decision_download (decision);
  return TRUE;
}

typedef struct {
  EphyWebView *web_view;
  WebKitPermissionRequest *request;
  char *origin;
} PermissionRequestData;

static PermissionRequestData *
permission_request_data_new (EphyWebView             *web_view,
                             WebKitPermissionRequest *request,
                             const char              *origin)
{
  PermissionRequestData *data;
  data = g_new (PermissionRequestData, 1);
  data->web_view = web_view;
  /* Ref the decision to keep it alive while we decide */
  data->request = g_object_ref (request);
  data->origin = g_strdup (origin);
  return data;
}

static void
permission_request_data_free (PermissionRequestData *data)
{
  g_object_unref (data->request);
  g_free (data->origin);
  g_free (data);
}

static void
permission_request_info_bar_destroyed_cb (PermissionRequestData *data,
                                          GObject               *where_the_info_bar_was)
{
  webkit_permission_request_deny (data->request);
  permission_request_data_free (data);
}

static void
decide_on_permission_request (GtkWidget             *info_bar,
                              int                    response,
                              PermissionRequestData *data)
{
  const char *address;
  EphyPermissionType permission_type = EPHY_PERMISSION_TYPE_SHOW_NOTIFICATIONS;

  switch (response) {
    case GTK_RESPONSE_YES:
      webkit_permission_request_allow (data->request);
      break;
    default:
      webkit_permission_request_deny (data->request);
      break;
  }

  if (WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST (data->request)) {
    permission_type = EPHY_PERMISSION_TYPE_ACCESS_LOCATION;
  } else if (WEBKIT_IS_NOTIFICATION_PERMISSION_REQUEST (data->request)) {
    permission_type = EPHY_PERMISSION_TYPE_SHOW_NOTIFICATIONS;
  } else if (WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST (data->request)) {
    gboolean is_for_audio_device = webkit_user_media_permission_is_for_audio_device (WEBKIT_USER_MEDIA_PERMISSION_REQUEST (data->request));
    gboolean is_for_video_device = webkit_user_media_permission_is_for_video_device (WEBKIT_USER_MEDIA_PERMISSION_REQUEST (data->request));

    if (is_for_audio_device) {
      if (is_for_video_device)
        permission_type = EPHY_PERMISSION_TYPE_ACCESS_WEBCAM_AND_MICROPHONE;
      else
        permission_type = EPHY_PERMISSION_TYPE_ACCESS_MICROPHONE;
    } else if (is_for_video_device) {
      permission_type = EPHY_PERMISSION_TYPE_ACCESS_WEBCAM;
    }
  } else {
    g_assert_not_reached ();
  }

  address = ephy_web_view_get_address (data->web_view);

  if (response != GTK_RESPONSE_NONE && ephy_embed_utils_address_has_web_scheme (address)) {
    EphyEmbedShell *shell;
    EphyPermissionsManager *permissions_manager;

    shell = ephy_embed_shell_get_default ();
    permissions_manager = ephy_embed_shell_get_permissions_manager (shell);

    if (permission_type != EPHY_PERMISSION_TYPE_ACCESS_WEBCAM_AND_MICROPHONE) {
      ephy_permissions_manager_set_permission (permissions_manager,
                                               permission_type,
                                               data->origin,
                                               response == GTK_RESPONSE_YES ? EPHY_PERMISSION_PERMIT
                                                                            : EPHY_PERMISSION_DENY);
    } else {
      ephy_permissions_manager_set_permission (permissions_manager,
                                               EPHY_PERMISSION_TYPE_ACCESS_WEBCAM,
                                               data->origin,
                                               response == GTK_RESPONSE_YES ? EPHY_PERMISSION_PERMIT
                                                                            : EPHY_PERMISSION_DENY);
      ephy_permissions_manager_set_permission (permissions_manager,
                                               EPHY_PERMISSION_TYPE_ACCESS_MICROPHONE,
                                               data->origin,
                                               response == GTK_RESPONSE_YES ? EPHY_PERMISSION_PERMIT
                                                                            : EPHY_PERMISSION_DENY);
    }
  }

  g_object_weak_unref (G_OBJECT (info_bar), (GWeakNotify)permission_request_info_bar_destroyed_cb, data);
  gtk_widget_destroy (info_bar);
  permission_request_data_free (data);
}

static void
show_permission_request_info_bar (WebKitWebView           *web_view,
                                  WebKitPermissionRequest *decision,
                                  EphyPermissionType       permission_type)
{
  PermissionRequestData *data;
  GtkWidget *info_bar;
  GtkWidget *content_area;
  GtkWidget *label;
  char *message;
  char *origin;
  char *bold_origin;

  info_bar = gtk_info_bar_new_with_buttons (_("Deny"), GTK_RESPONSE_NO,
                                            _("Allow"), GTK_RESPONSE_YES,
                                            NULL);

  /* Label */
  origin = ephy_uri_to_security_origin (webkit_web_view_get_uri (web_view));
  if (origin == NULL)
    return;

  bold_origin = g_markup_printf_escaped ("<b>%s</b>", origin);

  switch (permission_type) {
    case EPHY_PERMISSION_TYPE_SHOW_NOTIFICATIONS:
      /* Translators: Notification policy for a specific site. */
      message = g_strdup_printf (_("The page at %s wants to show desktop notifications."),
                                 bold_origin);
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_LOCATION:
      /* Translators: Geolocation policy for a specific site. */
      message = g_strdup_printf (_("The page at %s wants to know your location."),
                                 bold_origin);
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_MICROPHONE:
      /* Translators: Microphone policy for a specific site. */
      message = g_strdup_printf (_("The page at %s wants to use your microphone."),
                                 bold_origin);
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_WEBCAM:
      /* Translators: Webcam policy for a specific site. */
      message = g_strdup_printf (_("The page at %s wants to use your webcam."),
                                 bold_origin);
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_WEBCAM_AND_MICROPHONE:
      /* Translators: Webcam and microphone policy for a specific site. */
      message = g_strdup_printf (_("The page at %s wants to use your webcam and microphone."),
                                 bold_origin);
      break;
    case EPHY_PERMISSION_TYPE_SAVE_PASSWORD:
    default:
      g_assert_not_reached ();
  }

  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), message);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0);

  content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (info_bar));
  gtk_container_add (GTK_CONTAINER (content_area), label);

  gtk_widget_show_all (info_bar);

  data = permission_request_data_new (EPHY_WEB_VIEW (web_view), decision, origin);

  g_signal_connect (info_bar, "response",
                    G_CALLBACK (decide_on_permission_request),
                    data);
  g_object_weak_ref (G_OBJECT (info_bar), (GWeakNotify)permission_request_info_bar_destroyed_cb, data);

  switch (permission_type) {
    case EPHY_PERMISSION_TYPE_SHOW_NOTIFICATIONS:
      track_info_bar (info_bar, &EPHY_WEB_VIEW (web_view)->notification_info_bar);
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_LOCATION:
      track_info_bar (info_bar, &EPHY_WEB_VIEW (web_view)->geolocation_info_bar);
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_MICROPHONE:
      track_info_bar (info_bar, &EPHY_WEB_VIEW (web_view)->microphone_info_bar);
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_WEBCAM:
      track_info_bar (info_bar, &EPHY_WEB_VIEW (web_view)->webcam_info_bar);
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_WEBCAM_AND_MICROPHONE:
      track_info_bar (info_bar, &EPHY_WEB_VIEW (web_view)->webcam_mic_info_bar);
      break;
    case EPHY_PERMISSION_TYPE_SAVE_PASSWORD:
    default:
      g_assert_not_reached ();
  }

  ephy_embed_add_top_widget (EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (web_view),
                             info_bar,
                             EPHY_EMBED_TOP_WIDGET_POLICY_DESTROY_ON_TRANSITION);

  g_free (message);
  g_free (origin);
  g_free (bold_origin);
}

static void
decide_on_itp_permission_request (GtkWidget               *info_bar,
                                  int                      response,
                                  WebKitPermissionRequest *request)
{
  switch (response) {
    case GTK_RESPONSE_YES:
      webkit_permission_request_allow (request);
      break;
    default:
      webkit_permission_request_deny (request);
      break;
  }

  g_object_set_data (G_OBJECT (info_bar), "ephy-itp-decision", NULL);
  gtk_widget_destroy (info_bar);
}

static void
ephy_web_view_show_itp_permission_info_bar (EphyWebView                              *web_view,
                                            WebKitWebsiteDataAccessPermissionRequest *decision)
{
  GtkWidget *info_bar;
  GtkWidget *content_area;
  GtkWidget *box;
  GtkWidget *label;
  g_autofree char *message = NULL;
  g_autofree char *secondary_message = NULL;
  g_autofree char *markup = NULL;
  const char *requesting_domain;
  const char *current_domain;

  info_bar = gtk_info_bar_new_with_buttons (_("Deny"), GTK_RESPONSE_NO,
                                            _("Allow"), GTK_RESPONSE_YES,
                                            NULL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  requesting_domain = webkit_website_data_access_permission_request_get_requesting_domain (decision);
  current_domain = webkit_website_data_access_permission_request_get_current_domain (decision);
  message = g_strdup_printf (_("Do you want to allow “%s” to use cookies while browsing “%s”?"), requesting_domain, current_domain);
  markup = g_strdup_printf ("<span weight='bold'>%s</span>", message);
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), markup);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_widget_show (label);

  secondary_message = g_strdup_printf (_("This will allow “%s” to track your activity."), requesting_domain);
  label = gtk_label_new (secondary_message);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_widget_show (label);

  content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (info_bar));
  gtk_container_add (GTK_CONTAINER (content_area), box);
  gtk_widget_show (box);

  track_info_bar (info_bar, &web_view->itp_info_bar);

  g_signal_connect (info_bar, "response",
                    G_CALLBACK (decide_on_itp_permission_request),
                    decision);
  g_object_set_data_full (G_OBJECT (info_bar), "ephy-itp-decision", g_object_ref (decision), g_object_unref);

  ephy_embed_add_top_widget (EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (web_view),
                             info_bar,
                             EPHY_EMBED_TOP_WIDGET_POLICY_DESTROY_ON_TRANSITION);
  gtk_widget_show (info_bar);
}

static gboolean
permission_request_cb (WebKitWebView           *web_view,
                       WebKitPermissionRequest *decision)
{
  const char *address;
  char *origin;
  EphyEmbedShell *shell;
  EphyPermissionsManager *permissions_manager;
  EphyPermission permission;
  EphyPermissionType permission_type = EPHY_PERMISSION_TYPE_SHOW_NOTIFICATIONS;

  shell = ephy_embed_shell_get_default ();

  if (WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST (decision)) {
    permission_type = EPHY_PERMISSION_TYPE_ACCESS_LOCATION;
  } else if (WEBKIT_IS_NOTIFICATION_PERMISSION_REQUEST (decision)) {
    permission_type = EPHY_PERMISSION_TYPE_SHOW_NOTIFICATIONS;
  } else if (WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST (decision)) {
    gboolean is_for_audio_device = webkit_user_media_permission_is_for_audio_device (WEBKIT_USER_MEDIA_PERMISSION_REQUEST (decision));
    gboolean is_for_video_device = webkit_user_media_permission_is_for_video_device (WEBKIT_USER_MEDIA_PERMISSION_REQUEST (decision));

    if (is_for_audio_device) {
      if (is_for_video_device)
        permission_type = EPHY_PERMISSION_TYPE_ACCESS_WEBCAM_AND_MICROPHONE;
      else
        permission_type = EPHY_PERMISSION_TYPE_ACCESS_MICROPHONE;
    } else if (is_for_video_device) {
      permission_type = EPHY_PERMISSION_TYPE_ACCESS_WEBCAM;
    } else {
      return FALSE;
    }
  } else if (WEBKIT_IS_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST (decision)) {
    ephy_web_view_show_itp_permission_info_bar (EPHY_WEB_VIEW (web_view),
                                                WEBKIT_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST (decision));
    return TRUE;
  } else {
    return FALSE;
  }

  address = ephy_web_view_get_address (EPHY_WEB_VIEW (web_view));
  origin = ephy_uri_to_security_origin (address);
  if (origin == NULL)
    return FALSE;

  permissions_manager = ephy_embed_shell_get_permissions_manager (ephy_embed_shell_get_default ());

  if (permission_type != EPHY_PERMISSION_TYPE_ACCESS_WEBCAM_AND_MICROPHONE) {
    permission = ephy_permissions_manager_get_permission (permissions_manager,
                                                          permission_type,
                                                          origin);
  } else {
    EphyPermission video_permission;
    EphyPermission mic_permission;

    video_permission = ephy_permissions_manager_get_permission (permissions_manager,
                                                                EPHY_PERMISSION_TYPE_ACCESS_WEBCAM,
                                                                origin);
    mic_permission = ephy_permissions_manager_get_permission (permissions_manager,
                                                              EPHY_PERMISSION_TYPE_ACCESS_MICROPHONE,
                                                              origin);

    if (video_permission == mic_permission)
      permission = video_permission;
    else
      permission = EPHY_PERMISSION_UNDECIDED;
  }

  switch (permission) {
    case EPHY_PERMISSION_PERMIT:
      webkit_permission_request_allow (decision);
      goto out;
    case EPHY_PERMISSION_DENY:
      webkit_permission_request_deny (decision);
      goto out;
    case EPHY_PERMISSION_UNDECIDED:
      /* Application mode implies being OK with notifications. */
      if (permission_type == EPHY_PERMISSION_TYPE_SHOW_NOTIFICATIONS &&
          ephy_embed_shell_get_mode (shell) == EPHY_EMBED_SHELL_MODE_APPLICATION) {
        ephy_permissions_manager_set_permission (permissions_manager,
                                                 permission_type,
                                                 origin,
                                                 EPHY_PERMISSION_PERMIT);
        webkit_permission_request_allow (decision);
      } else {
        show_permission_request_info_bar (web_view, decision, permission_type);
      }
  }

out:
  g_free (origin);

  return TRUE;
}

static void
get_host_for_url_cb (gpointer service,
                     gboolean success,
                     gpointer result_data,
                     gpointer user_data)
{
  EphyHistoryHost *host;
  EphyWebView *view;
  double current_zoom;
  double set_zoom;

  if (success == FALSE)
    return;

  view = EPHY_WEB_VIEW (user_data);
  host = (EphyHistoryHost *)result_data;

  current_zoom = webkit_web_view_get_zoom_level (WEBKIT_WEB_VIEW (view));

  /* Use default zoom level in case web page is
   *  - not visited before
   *  - uses default zoom level (0)
   */
  if (host->visit_count == 0 || host->zoom_level == 0.0) {
    set_zoom = g_settings_get_double (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_DEFAULT_ZOOM_LEVEL);
  } else {
    set_zoom = host->zoom_level;
  }

  if (set_zoom != current_zoom) {
    view->is_setting_zoom = TRUE;
    webkit_web_view_set_zoom_level (WEBKIT_WEB_VIEW (view), set_zoom);
    view->is_setting_zoom = FALSE;
  }
}

static void
restore_zoom_level (EphyWebView *view,
                    const char  *address)
{
  if (ephy_embed_utils_address_has_web_scheme (address))
    ephy_history_service_get_host_for_url (view->history_service,
                                           address, view->cancellable,
                                           (EphyHistoryJobCallback)get_host_for_url_cb, view);
}

static void
ephy_web_view_set_loading_message (EphyWebView *view,
                                   const char  *address)
{
  g_clear_pointer (&view->loading_message, g_free);
  if (address) {
    char *decoded_address;
    char *title;

    decoded_address = ephy_uri_decode (address);
    title = ephy_embed_utils_get_title_from_address (decoded_address);

    if (title != NULL && title[0] != '\0') {
      /* translators: %s here is the address of the web page */
      view->loading_message = g_strdup_printf (_("Loading “%s”…"), title);
    } else {
      view->loading_message = g_strdup (_("Loading…"));
    }

    g_free (decoded_address);
    g_free (title);
  } else {
    view->loading_message = g_strdup (_("Loading…"));
  }

  g_object_notify_by_pspec (G_OBJECT (view), obj_properties[PROP_STATUS_MESSAGE]);
}

static void
ephy_web_view_unset_loading_message (EphyWebView *view)
{
  g_clear_pointer (&view->loading_message, g_free);
  g_object_notify_by_pspec (G_OBJECT (view), obj_properties[PROP_STATUS_MESSAGE]);
}

static void
ephy_web_view_set_committed_location (EphyWebView *view,
                                      const char  *location)
{
  GObject *object = G_OBJECT (view);

  g_object_freeze_notify (object);

  /* Do this up here so we still have the old address around. */
  ephy_file_monitor_update_location (view->file_monitor, location);

  if (location == NULL || location[0] == '\0') {
    ephy_web_view_set_address (view, NULL);
  } else if (g_str_has_prefix (location, EPHY_ABOUT_SCHEME ":applications")) {
    g_autoptr (GUri) uri = NULL;
    g_autofree char *new_address = NULL;

    /* Strip the query from the URL for about:applications. */
    uri = g_uri_parse (location, G_URI_FLAGS_NONE, NULL);
    new_address = g_uri_to_string_partial (uri, G_URI_HIDE_QUERY);
    ephy_web_view_set_address (view, new_address);
  } else {
    /* We do this to get rid of an eventual password in the URL. */
    ephy_web_view_set_address (view, location);
    ephy_web_view_set_loading_message (view, location);
  }

  g_clear_pointer (&view->last_committed_address, g_free);
  view->last_committed_address = g_strdup (view->address);

  ephy_web_view_set_link_message (view, NULL);

  _ephy_web_view_update_icon (view);

  g_object_thaw_notify (object);
}

static char *
hostname_to_tld (const char *hostname)
{
  g_auto (GStrv) parts = NULL;
  guint length;

  parts = g_strsplit (hostname, ".", 0);
  length = g_strv_length (parts);

  if (length >= 1)
    return g_strdup (parts[length - 1]);

  return g_strdup ("");
}

static void
update_security_status_for_committed_load (EphyWebView *view,
                                           const char  *uri)
{
  EphySecurityLevel security_level = EPHY_SECURITY_LEVEL_NO_SECURITY;
  EphyEmbed *embed = NULL;
  GtkWidget *toplevel;
  WebKitWebContext *web_context;
  WebKitSecurityManager *security_manager;
  g_autoptr (GUri) guri = NULL;
  g_autofree char *tld = NULL;

  if (view->loading_error_page)
    return;

  if (g_str_has_prefix (uri, "ephy-webextension://")) {
    /* Hidden WebExtension webview, ignoring */
    return;
  }

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));
  if (EPHY_IS_EMBED_CONTAINER (toplevel))
    embed = EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view);
  web_context = webkit_web_view_get_context (WEBKIT_WEB_VIEW (view));
  security_manager = webkit_web_context_get_security_manager (web_context);
  guri = g_uri_parse (uri, G_URI_FLAGS_NONE, NULL);

  g_clear_object (&view->certificate);
  g_clear_pointer (&view->tls_error_failing_uri, g_free);

  if (guri && g_uri_get_host (guri))
    tld = hostname_to_tld (g_uri_get_host (guri));

  if (!guri ||
      strcmp (g_uri_get_scheme (guri), EPHY_VIEW_SOURCE_SCHEME) == 0 ||
      strcmp (g_uri_get_scheme (guri), EPHY_READER_SCHEME) == 0 ||
      strcmp (g_uri_get_scheme (guri), EPHY_PDF_SCHEME) == 0 ||
      g_strcmp0 (tld, "127.0.0.1") == 0 ||
      g_strcmp0 (tld, "::1") == 0 ||
      g_strcmp0 (tld, "localhost") == 0 || /* We trust localhost to be local since glib!616. */
      webkit_security_manager_uri_scheme_is_local (security_manager, g_uri_get_scheme (guri)) ||
      webkit_security_manager_uri_scheme_is_empty_document (security_manager, g_uri_get_scheme (guri))) {
    security_level = EPHY_SECURITY_LEVEL_LOCAL_PAGE;
  } else if (webkit_web_view_get_tls_info (WEBKIT_WEB_VIEW (view), &view->certificate, &view->tls_errors)) {
    g_object_ref (view->certificate);
    security_level = view->tls_errors == 0 ?
                     EPHY_SECURITY_LEVEL_STRONG_SECURITY : EPHY_SECURITY_LEVEL_UNACCEPTABLE_CERTIFICATE;
  } else if (!embed || ephy_embed_has_load_pending (embed)) {
    security_level = EPHY_SECURITY_LEVEL_TO_BE_DETERMINED;
  }

  ephy_web_view_set_security_level (view, security_level);
}

static void
update_ucm_ads_state (WebKitWebView *web_view,
                      const char    *uri)
{
  WebKitUserContentManager *ucm = webkit_web_view_get_user_content_manager (web_view);
  EphyPermission permission = EPHY_PERMISSION_UNDECIDED;
  gboolean enable = FALSE;
  g_autofree gchar *origin = NULL;
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();

  origin = ephy_uri_to_security_origin (uri);

  /* Check page setting first in case it overwrites global setting */
  if (origin)
    permission = ephy_permissions_manager_get_permission (ephy_embed_shell_get_permissions_manager (shell),
                                                          EPHY_PERMISSION_TYPE_SHOW_ADS,
                                                          origin);
  enable = permission == EPHY_PERMISSION_DENY;
  if (permission == EPHY_PERMISSION_UNDECIDED && g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_ADBLOCK))
    enable = TRUE;

  ephy_filters_manager_set_ucm_forbids_ads (ephy_embed_shell_get_filters_manager (shell), ucm, enable);
}

static void
reset_background_color (WebKitWebView *web_view)
{
  GdkRGBA white = { 1.0, 1.0, 1.0, 1.0 };

  /* https://bugs.webkit.org/show_bug.cgi?id=206953#c2 */
  webkit_web_view_set_background_color (web_view, &white);
}

static void
load_changed_cb (WebKitWebView   *web_view,
                 WebKitLoadEvent  load_event,
                 gpointer         user_data)
{
  EphyWebView *view = EPHY_WEB_VIEW (web_view);
  GObject *object = G_OBJECT (web_view);

  g_object_freeze_notify (object);

  view->in_auth_dialog = 0;

  /* Warning: the URI property may remain set to the URI of the
   * previously-loaded page until WEBKIT_LOAD_COMMITTED! During
   * WEBKIT_LOAD_STARTED, it may or may not match the URI being loaded.
   */
  switch (load_event) {
    case WEBKIT_LOAD_STARTED: {
      view->load_failed = FALSE;
      view->document_type = EPHY_WEB_VIEW_DOCUMENT_HTML;

      g_clear_handle_id (&view->snapshot_timeout_id, g_source_remove);

      if (view->address == NULL || view->address[0] == '\0') {
        /* We've probably never loaded any page before. */
        ephy_web_view_set_address (view, webkit_web_view_get_uri (web_view));
      }

      ephy_web_view_set_loading_message (view, NULL);

      break;
    }
    case WEBKIT_LOAD_REDIRECTED:
      break;
    case WEBKIT_LOAD_COMMITTED: {
      const char *uri;
      view->ever_committed = TRUE;

      /* Title and location. */
      uri = webkit_web_view_get_uri (web_view);
      ephy_web_view_set_committed_location (view, uri);
      update_security_status_for_committed_load (view, uri);
      update_ucm_ads_state (web_view, uri);

      /* History. */
      if (ephy_embed_utils_is_no_show_address (uri))
        ephy_web_view_freeze_history (view);

      if (!ephy_web_view_is_history_frozen (view)) {
        char *history_uri = NULL;

        /* TODO: move the normalization down to the history service? */
        if (g_str_has_prefix (uri, EPHY_ABOUT_SCHEME))
          history_uri = g_strdup_printf ("about:%s", uri + EPHY_ABOUT_SCHEME_LEN + 1);
        else if (g_str_has_prefix (uri, EPHY_PDF_SCHEME))
          history_uri = g_strdup (uri + strlen (EPHY_PDF_SCHEME) + 1);
        else
          history_uri = g_strdup (uri);

        ephy_history_service_visit_url (view->history_service,
                                        history_uri,
                                        NULL,
                                        g_get_real_time (),
                                        view->visit_type,
                                        TRUE);

        g_free (history_uri);
      }

      if (view->loading_error_page)
        view->loading_error_page = FALSE;
      else
        view->error_page = EPHY_WEB_VIEW_ERROR_PAGE_NONE;

      /* Zoom level. */
      restore_zoom_level (view, uri);

      /* We have to reset the background color here because we set a
       * nonwhite background in constructed.
       */
      if (!g_str_has_prefix (uri, EPHY_ABOUT_SCHEME))
        reset_background_color (web_view);

      break;
    }
    case WEBKIT_LOAD_FINISHED:
      ephy_web_view_unset_loading_message (view);

      /* Ensure we load the icon for this web view, if available. */
      _ephy_web_view_update_icon (view);

      if (g_str_has_prefix (webkit_web_view_get_uri (web_view), "ephy-pdf"))
        ephy_embed_shell_pdf_handler_stop (ephy_embed_shell_get_default (), web_view);

      /* Reset visit type. */
      view->visit_type = EPHY_PAGE_VISIT_NONE;

      if (view->entering_reader_mode) {
        view->entering_reader_mode = FALSE;
        g_object_notify_by_pspec (G_OBJECT (web_view), obj_properties[PROP_ENTERING_READER_MODE]);
      }

      if (!ephy_web_view_is_history_frozen (view) &&
          ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) != EPHY_EMBED_SHELL_MODE_INCOGNITO) {
        /* FIXME: The 1s delay is a workaround to allow time to render the page and get a favicon.
         * https://bugzilla.gnome.org/show_bug.cgi?id=761065
         * https://bugs.webkit.org/show_bug.cgi?id=164180
         */
        if (view->snapshot_timeout_id == 0) {
          view->snapshot_timeout_id = g_timeout_add_seconds_full (G_PRIORITY_LOW, 1,
                                                                  (GSourceFunc)maybe_take_snapshot,
                                                                  web_view, NULL);
          g_free (view->pending_snapshot_uri);
          view->pending_snapshot_uri = g_strdup (webkit_web_view_get_uri (WEBKIT_WEB_VIEW (view)));
        }
      }

      ephy_web_view_thaw_history (view);

      g_clear_handle_id (&view->reader_js_timeout, g_source_remove);

      if (!ephy_embed_utils_is_no_show_address (view->address))
        view->reader_js_timeout = g_idle_add (run_readability_js_if_needed, web_view);

      break;

    default:
      break;
  }

  g_object_thaw_notify (object);
}

/**
 * ephy_web_view_set_placeholder:
 * @view: an #EphyWebView
 * @uri: uri that will eventually be loaded
 * @title: last-known title of the page that will eventually be loaded
 *
 * Makes the #EphyWebView pretend a page that will eventually be loaded is
 * already there.
 *
 **/
void
ephy_web_view_set_placeholder (EphyWebView *view,
                               const char  *uri,
                               const char  *title)
{
  char *html;

  g_assert (EPHY_IS_WEB_VIEW (view));

  /* We want only the actual load to be the one recorded in history, but
   * doing a load here is the simplest way to replace the loading
   * spinner with the favicon. */
  ephy_web_view_freeze_history (view);

  html = g_markup_printf_escaped ("<head><title>%s</title></head>", title);

  webkit_web_view_load_alternate_html (WEBKIT_WEB_VIEW (view), html, uri, NULL);

  g_free (html);

  ephy_web_view_set_address (view, uri);
}

static char *
get_style_sheet (void)
{
  GBytes *bytes;
  char *sheet;

  bytes = g_resources_lookup_data (EPHY_PAGE_TEMPLATE_ERROR_CSS, 0, NULL);
  sheet = g_strdup (g_bytes_get_data (bytes, NULL));
  g_bytes_unref (bytes);

  return sheet;
}

static char *
detailed_message_from_tls_errors (GTlsCertificateFlags tls_errors)
{
  GPtrArray *errors = g_ptr_array_new ();
  char *retval = NULL;

  if (tls_errors & G_TLS_CERTIFICATE_BAD_IDENTITY) {
    /* Possible error message when a site presents a bad certificate. */
    g_ptr_array_add (errors, _("This website presented identification that belongs to a different website."));
  }

  if (tls_errors & G_TLS_CERTIFICATE_EXPIRED) {
    /* Possible error message when a site presents a bad certificate. */
    g_ptr_array_add (errors, _("This website’s identification is too old to trust. Check the date on your computer’s calendar."));
  }

  if (tls_errors & G_TLS_CERTIFICATE_UNKNOWN_CA) {
    /* Possible error message when a site presents a bad certificate. */
    g_ptr_array_add (errors, _("This website’s identification was not issued by a trusted organization."));
  }

  if (tls_errors & G_TLS_CERTIFICATE_GENERIC_ERROR) {
    /* Possible error message when a site presents a bad certificate. */
    g_ptr_array_add (errors, _("This website’s identification could not be processed. It may be corrupted."));
  }

  if (tls_errors & G_TLS_CERTIFICATE_REVOKED) {
    /* Possible error message when a site presents a bad certificate. */
    g_ptr_array_add (errors, _("This website’s identification has been revoked by the trusted organization that issued it."));
  }

  if (tls_errors & G_TLS_CERTIFICATE_INSECURE) {
    /* Possible error message when a site presents a bad certificate. */
    g_ptr_array_add (errors, _("This website’s identification cannot be trusted because it uses very weak encryption."));
  }

  if (tls_errors & G_TLS_CERTIFICATE_NOT_ACTIVATED) {
    /* Possible error message when a site presents a bad certificate. */
    g_ptr_array_add (errors, _("This website’s identification is only valid for future dates. Check the date on your computer’s calendar."));
  }

  if (errors->len == 1) {
    retval = g_strdup (g_ptr_array_index (errors, 0));
  } else if (errors->len > 1) {
    GString *message = g_string_new ("<ul>");
    guint i;

    for (i = 0; i < errors->len; i++) {
      g_string_append_printf (message, "<li>%s</li>", (char *)g_ptr_array_index (errors, i));
    }

    g_string_append (message, "</ul>");
    retval = g_string_free (message, FALSE);
  } else {
    g_assert_not_reached ();
  }

  g_ptr_array_free (errors, TRUE);

  return retval;
}

/**
 * ephy_web_view_get_error_page:
 * @view: an #EphyWebView
 *
 * Returns the error page currently displayed, or
 * %EPHY_WEB_VIEW_ERROR_PAGE_NONE.
 *
 **/
EphyWebViewErrorPage
ephy_web_view_get_error_page (EphyWebView *view)
{
  g_assert (EPHY_IS_WEB_VIEW (view));

  return view->error_page;
}

/* Note we go to some effort to avoid error-prone markup in translatable
 * strings. Everywhere, but also here on the error pages in particular. */

static void
format_network_error_page (EphyWebView  *view,
                           const char   *uri,
                           const char   *origin,
                           const char   *reason,
                           char        **page_title,
                           char        **message_title,
                           char        **message_body,
                           char        **message_details,
                           char        **button_label,
                           char        **button_action,
                           const char  **button_accesskey,
                           const char  **icon_name,
                           const char  **style)
{
  g_autofree char *encoded_origin = NULL;
  g_autofree char *formatted_origin = NULL;
  g_autofree char *formatted_reason = NULL;
  g_autofree char *first_paragraph = NULL;
  const char *second_paragraph;

  /* Page title when a site cannot be loaded due to a network error. */
  *page_title = g_strdup_printf (_("Problem Loading Page"));

  /* Message title when a site cannot be loaded due to a network error. */
  *message_title = g_strdup (_("Unable to display this website"));

  encoded_origin = ephy_encode_for_html_entity (origin);
  formatted_origin = g_strdup_printf ("<strong>%s</strong>", encoded_origin);
  /* Error details when a site cannot be loaded due to a network error. */
  first_paragraph = g_strdup_printf (_("The site at %s seems to be "
                                       "unavailable."),
                                     formatted_origin);
  /* Further error details when a site cannot be loaded due to a network error. */
  second_paragraph = _("It may be temporarily inaccessible or moved to a new "
                       "address. You may wish to verify that your internet "
                       "connection is working correctly.");
  *message_body = g_strdup_printf ("<p>%s</p><p>%s</p>",
                                   first_paragraph,
                                   second_paragraph);

  formatted_reason = g_strdup_printf ("<i>%s</i>", reason);
  g_free (first_paragraph);
  /* Technical details when a site cannot be loaded due to a network error. */
  first_paragraph = g_strdup_printf (_("The precise error was: %s"),
                                     formatted_reason);
  *message_details = g_strdup_printf ("<p>%s</p>", first_paragraph);

  /* The button on the network error page. DO NOT ADD MNEMONICS HERE. */
  *button_label = g_strdup (_("Reload"));
  *button_action = g_strdup_printf ("window.webkit.messageHandlers.reloadPage.postMessage(%" G_GUINT64_FORMAT ");",
                                    webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)));
  /* Mnemonic for the Reload button on browser error pages. */
  *button_accesskey = C_("reload-access-key", "R");

  *icon_name = "network-error-symbolic.svg";
  *style = "default";
}

static void
format_crash_error_page (EphyWebView  *view,
                         const char   *uri,
                         char        **page_title,
                         char        **message_title,
                         char        **message_body,
                         char        **button_label,
                         char        **button_action,
                         const char  **button_accesskey,
                         const char  **icon_name,
                         const char  **style)
{
  g_autofree char *encoded_uri = NULL;
  g_autofree char *formatted_uri = NULL;
  g_autofree char *formatted_distributor = NULL;
  g_autofree char *first_paragraph = NULL;
  g_autofree char *second_paragraph = NULL;

  /* Page title when a site cannot be loaded due to a page crash error. */
  *page_title = g_strdup_printf (_("Problem Loading Page"));

  /* Message title when a site cannot be loaded due to a page crash error. */
  *message_title = g_strdup (_("Oops! There may be a problem"));

  encoded_uri = ephy_encode_for_html_entity (uri);
  formatted_uri = g_strdup_printf ("<strong>%s</strong>", encoded_uri);
  /* Error details when a site cannot be loaded due to a page crash error. */
  first_paragraph = g_strdup_printf (_("The page %s may have caused Web to "
                                       "close unexpectedly."),
                                     formatted_uri);

  formatted_distributor = g_strdup_printf ("<strong>%s</strong>",
                                           DISTRIBUTOR_NAME);
  /* Further error details when a site cannot be loaded due to a page crash error. */
  second_paragraph = g_strdup_printf (_("If this happens again, please report "
                                        "the problem to the %s developers."),
                                      formatted_distributor);

  *message_body = g_strdup_printf ("<p>%s</p><p>%s</p>",
                                   first_paragraph,
                                   second_paragraph);

  /* The button on the page crash error page. DO NOT ADD MNEMONICS HERE. */
  *button_label = g_strdup (_("Reload"));
  *button_action = g_strdup_printf ("window.webkit.messageHandlers.reloadPage.postMessage(%" G_GUINT64_FORMAT ");",
                                    webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)));
  /* Mnemonic for the Reload button on browser error pages. */
  *button_accesskey = C_("reload-access-key", "R");

  *icon_name = "computer-fail-symbolic.svg";
  *style = "default";
}

static void
format_process_crash_error_page (EphyWebView  *view,
                                 const char   *uri,
                                 char        **page_title,
                                 char        **message_title,
                                 char        **message_body,
                                 char        **button_label,
                                 char        **button_action,
                                 const char  **button_accesskey,
                                 const char  **icon_name,
                                 const char  **style)
{
  const char *first_paragraph;

  /* Page title when a site cannot be loaded due to a process crash error. */
  *page_title = g_strdup_printf (_("Problem Displaying Page"));

  /* Message title when a site cannot be loaded due to a process crash error. */
  *message_title = g_strdup (_("Oops!"));

  /* Error details when a site cannot be loaded due to a process crash error. */
  first_paragraph = _("Something went wrong while displaying this page. Please reload or visit a different page to continue.");
  *message_body = g_strdup_printf ("<p>%s</p>",
                                   first_paragraph);

  /* The button on the process crash error page. DO NOT ADD MNEMONICS HERE. */
  *button_label = g_strdup (_("Reload"));
  *button_action = g_strdup_printf ("window.webkit.messageHandlers.reloadPage.postMessage(%" G_GUINT64_FORMAT ");",
                                    webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)));
  /* Mnemonic for the Reload button on browser error pages. */
  *button_accesskey = C_("reload-access-key", "R");

  *icon_name = "computer-fail-symbolic.svg";
  *style = "default";
}

static void
format_unresponsive_process_error_page (EphyWebView  *view,
                                        const char   *uri,
                                        char        **page_title,
                                        char        **message_title,
                                        char        **message_body,
                                        char        **button_label,
                                        char        **button_action,
                                        const char  **button_accesskey,
                                        const char  **icon_name,
                                        const char  **style)
{
  const char *first_paragraph;

  /* Page title when web content has become unresponsive. */
  *page_title = g_strdup_printf (_("Unresponsive Page"));

  /* Message title when web content has become unresponsive. */
  *message_title = g_strdup (_("Uh-oh!"));

  /* Error details when web content has become unresponsive. */
  first_paragraph = _("This page has been unresponsive for too long. Please reload or visit a different page to continue.");
  *message_body = g_strdup_printf ("<p>%s</p>",
                                   first_paragraph);

  /* The button on the unresponsive process error page. DO NOT ADD MNEMONICS HERE. */
  *button_label = g_strdup (_("Reload"));
  *button_action = g_strdup_printf ("window.webkit.messageHandlers.reloadPage.postMessage(%" G_GUINT64_FORMAT ");",
                                    webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)));
  /* Mnemonic for the Reload button on browser error pages. */
  *button_accesskey = C_("reload-access-key", "R");

  *icon_name = "computer-fail-symbolic.svg";
  *style = "default";
}

static void
format_tls_error_page (EphyWebView  *view,
                       const char   *origin,
                       char        **page_title,
                       char        **message_title,
                       char        **message_body,
                       char        **message_details,
                       char        **button_label,
                       char        **button_action,
                       const char  **button_accesskey,
                       char        **hidden_button_label,
                       char        **hidden_button_action,
                       const char  **hidden_button_accesskey,
                       const char  **icon_name,
                       const char  **style)
{
  g_autofree char *encoded_origin = NULL;
  g_autofree char *formatted_origin = NULL;
  g_autofree char *first_paragraph = NULL;

  /* Page title when a site is not loaded due to an invalid TLS certificate. */
  *page_title = g_strdup_printf (_("Security Violation"));

  /* Message title when a site is not loaded due to an invalid TLS certificate. */
  *message_title = g_strdup (_("This Connection is Not Secure"));

  encoded_origin = ephy_encode_for_html_entity (origin);
  formatted_origin = g_strdup_printf ("<strong>%s</strong>", encoded_origin);
  /* Error details when a site is not loaded due to an invalid TLS certificate. */
  first_paragraph = g_strdup_printf (_("This does not look like the real %s. "
                                       "Attackers might be trying to steal or "
                                       "alter information going to or from "
                                       "this site."),
                                     formatted_origin);

  *message_body = g_strdup_printf ("<p>%s</p>", first_paragraph);
  *message_details = detailed_message_from_tls_errors (view->tls_errors);

  /* The button on the invalid TLS certificate error page. DO NOT ADD MNEMONICS HERE. */
  *button_label = g_strdup (_("Go Back"));
  *button_action = g_strdup ("window.history.back();");
  /* Mnemonic for the Go Back button on the invalid TLS certificate error page. */
  *button_accesskey = C_("back-access-key", "B");

  /* The hidden button on the invalid TLS certificate error page. Do not add mnemonics here. */
  *hidden_button_label = g_strdup (_("Accept Risk and Proceed"));
  *hidden_button_action = g_strdup_printf ("window.webkit.messageHandlers.tlsErrorPage.postMessage(%"G_GUINT64_FORMAT ");",
                                           webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)));
  /* Mnemonic for the Accept Risk and Proceed button on the invalid TLS certificate error page. */
  *hidden_button_accesskey = C_("proceed-anyway-access-key", "P");

  *icon_name = "channel-insecure-symbolic.svg";
  *style = "danger";
}

static void
format_unsafe_browsing_error_page (EphyWebView  *view,
                                   const char   *origin,
                                   const char   *threat_type,
                                   char        **page_title,
                                   char        **message_title,
                                   char        **message_body,
                                   char        **message_details,
                                   char        **button_label,
                                   char        **button_action,
                                   const char  **button_accesskey,
                                   char        **hidden_button_label,
                                   char        **hidden_button_action,
                                   const char  **hidden_button_accesskey,
                                   const char  **icon_name,
                                   const char  **style)
{
  g_autofree char *encoded_origin = NULL;
  g_autofree char *formatted_origin = NULL;
  g_autofree char *first_paragraph = NULL;

  /* Page title when a site is flagged by Google Safe Browsing verification. */
  *page_title = g_strdup_printf (_("Security Warning"));

  /* Message title on the unsafe browsing error page. */
  *message_title = g_strdup (_("Unsafe website detected!"));

  encoded_origin = ephy_encode_for_html_entity (origin);
  formatted_origin = g_strdup_printf ("<strong>%s</strong>", encoded_origin);
  /* Error details on the unsafe browsing error page.
   * https://developers.google.com/safe-browsing/v4/usage-limits#UserWarnings
   */
  if (!g_strcmp0 (threat_type, GSB_THREAT_TYPE_MALWARE)) {
    first_paragraph = g_strdup_printf (_("Visiting %s may harm your computer. This "
                                         "page appears to contain malicious code that could "
                                         "be downloaded to your computer without your consent."),
                                       formatted_origin);
    *message_details = g_strdup_printf (_("You can learn more about harmful web content "
                                          "including viruses and other malicious code "
                                          "and how to protect your computer at %s."),
                                        "<a href=\"https://www.stopbadware.org/\">"
                                        "www.stopbadware.org"
                                        "</a>");
  } else if (!g_strcmp0 (threat_type, GSB_THREAT_TYPE_SOCIAL_ENGINEERING)) {
    first_paragraph = g_strdup_printf (_("Attackers on %s may trick you into doing "
                                         "something dangerous like installing software or "
                                         "revealing your personal information (for example, "
                                         "passwords, phone numbers, or credit cards)."),
                                       formatted_origin);
    *message_details = g_strdup_printf (_("You can find out more about social engineering "
                                          "(phishing) at %s or from %s."),
                                        "<a href=\"https://support.google.com/webmasters/answer/6350487\">"
                                        "Social Engineering (Phishing and Deceptive Sites)"
                                        "</a>",
                                        "<a href=\"https://www.antiphishing.org/\">"
                                        "www.antiphishing.org"
                                        "</a>");
  } else {
    first_paragraph = g_strdup_printf (_("%s may contain harmful programs. Attackers might "
                                         "attempt to trick you into installing programs that "
                                         "harm your browsing experience (for example, by changing "
                                         "your homepage or showing extra ads on sites you visit)."),
                                       formatted_origin);
    *message_details = g_strdup_printf (_("You can learn more about unwanted software at %s."),
                                        "<a href=\"https://www.google.com/about/unwanted-software-policy.html\">"
                                        "Unwanted Software Policy"
                                        "</a>");
  }

  *message_body = g_strdup_printf ("<p>%s</p>", first_paragraph);

  /* The button on unsafe browsing error page. DO NOT ADD MNEMONICS HERE. */
  *button_label = g_strdup (_("Go Back"));
  *button_action = g_strdup ("window.history.back();");
  /* Mnemonic for the Go Back button on the unsafe browsing error page. */
  *button_accesskey = C_("back-access-key", "B");

  /* The hidden button on the unsafe browsing error page. Do not add mnemonics here. */
  *hidden_button_label = g_strdup (_("Accept Risk and Proceed"));
  *hidden_button_action = g_strdup_printf ("window.webkit.messageHandlers.unsafeBrowsingErrorPage.postMessage(%"G_GUINT64_FORMAT ");",
                                           webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)));
  /* Mnemonic for the Accept Risk and Proceed button on the unsafe browsing error page. */
  *hidden_button_accesskey = C_("proceed-anyway-access-key", "P");

  *icon_name = "security-high-symbolic.svg";
  *style = "danger";
}

static void
format_no_such_file_error_page (EphyWebView  *view,
                                char        **page_title,
                                char        **message_title,
                                char        **message_body,
                                char        **button_label,
                                char        **button_action,
                                const char  **button_accesskey,
                                const char  **icon_name,
                                const char  **style)
{
  g_autofree gchar *encoded_address = NULL;
  g_autofree gchar *formatted_address = NULL;
  g_autofree gchar *first_paragraph = NULL;
  g_autofree gchar *second_paragraph = NULL;

  /* Page title on no such file error page */
  *page_title = g_strdup_printf (_("File not found"));

  /* Message title on the no such file error page. */
  *message_title = g_strdup (_("File not found"));

  encoded_address = ephy_encode_for_html_entity (view->address);
  formatted_address = g_strdup_printf ("<strong>%s</strong>", encoded_address);

  first_paragraph = g_strdup_printf (_("%s could not be found."),
                                     formatted_address);
  second_paragraph = g_strdup_printf (_("Please check the file name for "
                                        "capitalization or other typing errors. Also check if "
                                        "it has been moved, renamed, or deleted."));

  *message_body = g_strdup_printf ("<p>%s</p><p>%s</p>", first_paragraph, second_paragraph);

  /* The button on no such file error page. DO NOT ADD MNEMONICS HERE. */
  *button_label = g_strdup (_("Go Back"));
  *button_action = g_strdup ("window.history.back();");
  /* Mnemonic for the Go Back button on the no such file error page. */
  *button_accesskey = C_("back-access-key", "B");

  *icon_name = "computer-fail-symbolic.svg";
  *style = "default";
}

/**
 * ephy_web_view_load_error_page:
 * @view: an #EphyWebView
 * @uri: uri that caused the failure
 * @page: one of #EphyWebViewErrorPage
 * @error: a GError to inspect, or %NULL
 * @user_data: a pointer to additional data
 *
 * Loads an error page appropiate for @page in @view.
 *
 **/
void
ephy_web_view_load_error_page (EphyWebView          *view,
                               const char           *uri,
                               EphyWebViewErrorPage  page,
                               GError               *error,
                               gpointer              user_data)
{
  g_autoptr (GBytes) html_file = NULL;
  g_autoptr (GString) html = g_string_new (NULL);
  g_autofree char *origin = NULL;
  g_autofree char *lang = NULL;
  g_autofree char *page_title = NULL;
  g_autofree char *msg_title = NULL;
  g_autofree char *msg_body = NULL;
  g_autofree char *msg_details = NULL;
  g_autofree char *button_label = NULL;
  g_autofree char *hidden_button_label = NULL;
  g_autofree char *button_action = NULL;
  g_autofree char *hidden_button_action = NULL;
  g_autofree char *style_sheet = NULL;
  const char *button_accesskey = NULL;
  const char *hidden_button_accesskey = NULL;
  const char *icon_name = NULL;
  const char *style = NULL;
  const char *reason = NULL;

  g_assert (page != EPHY_WEB_VIEW_ERROR_PAGE_NONE);

  view->loading_error_page = TRUE;
  view->error_page = page;

  if (page == EPHY_WEB_VIEW_ERROR_INVALID_TLS_CERTIFICATE)
    ephy_web_view_set_security_level (view, EPHY_SECURITY_LEVEL_UNACCEPTABLE_CERTIFICATE);
  else
    ephy_web_view_set_security_level (view, EPHY_SECURITY_LEVEL_LOCAL_PAGE);

  reason = error ? error->message : _("None specified");

  origin = ephy_uri_to_security_origin (uri);
  if (origin == NULL)
    origin = g_strdup (uri);

  lang = g_strdup (pango_language_to_string (gtk_get_default_language ()));
  g_strdelimit (lang, "_-@", '\0');

  html_file = g_resources_lookup_data (EPHY_PAGE_TEMPLATE_ERROR, 0, NULL);

  switch (page) {
    case EPHY_WEB_VIEW_ERROR_PAGE_NETWORK_ERROR:
      format_network_error_page (view,
                                 uri,
                                 origin,
                                 reason,
                                 &page_title,
                                 &msg_title,
                                 &msg_body,
                                 &msg_details,
                                 &button_label,
                                 &button_action,
                                 &button_accesskey,
                                 &icon_name,
                                 &style);
      break;
    case EPHY_WEB_VIEW_ERROR_PAGE_CRASH:
      format_crash_error_page (view,
                               uri,
                               &page_title,
                               &msg_title,
                               &msg_body,
                               &button_label,
                               &button_action,
                               &button_accesskey,
                               &icon_name,
                               &style);
      break;
    case EPHY_WEB_VIEW_ERROR_PROCESS_CRASH:
      format_process_crash_error_page (view,
                                       uri,
                                       &page_title,
                                       &msg_title,
                                       &msg_body,
                                       &button_label,
                                       &button_action,
                                       &button_accesskey,
                                       &icon_name,
                                       &style);
      break;
    case EPHY_WEB_VIEW_ERROR_UNRESPONSIVE_PROCESS:
      format_unresponsive_process_error_page (view,
                                              uri,
                                              &page_title,
                                              &msg_title,
                                              &msg_body,
                                              &button_label,
                                              &button_action,
                                              &button_accesskey,
                                              &icon_name,
                                              &style);
      break;
    case EPHY_WEB_VIEW_ERROR_INVALID_TLS_CERTIFICATE:
      format_tls_error_page (view,
                             origin,
                             &page_title,
                             &msg_title,
                             &msg_body,
                             &msg_details,
                             &button_label,
                             &button_action,
                             &button_accesskey,
                             &hidden_button_label,
                             &hidden_button_action,
                             &hidden_button_accesskey,
                             &icon_name,
                             &style);
      break;
    case EPHY_WEB_VIEW_ERROR_UNSAFE_BROWSING:
      format_unsafe_browsing_error_page (view,
                                         origin,
                                         user_data,
                                         &page_title,
                                         &msg_title,
                                         &msg_body,
                                         &msg_details,
                                         &button_label,
                                         &button_action,
                                         &button_accesskey,
                                         &hidden_button_label,
                                         &hidden_button_action,
                                         &hidden_button_accesskey,
                                         &icon_name,
                                         &style);
      break;
    case EPHY_WEB_VIEW_ERROR_NO_SUCH_FILE:
      format_no_such_file_error_page (view,
                                      &page_title,
                                      &msg_title,
                                      &msg_body,
                                      &button_label,
                                      &button_action,
                                      &button_accesskey,
                                      &icon_name,
                                      &style);
      break;

    case EPHY_WEB_VIEW_ERROR_PAGE_NONE:
    default:
      g_assert_not_reached ();
  }

  _ephy_web_view_update_icon (view);

  style_sheet = get_style_sheet ();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
  /* The HTML file is trusted input. */
  g_string_printf (html,
                   g_bytes_get_data (html_file, NULL),
                   lang, lang,
                   ((gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL) ? "rtl" : "ltr"),
                   page_title,
                   style_sheet,
                   button_action, hidden_button_action,
                   style,
                   icon_name,
                   msg_title, msg_body,
                   msg_details ? "visible" : "hidden",
                   _("Technical information"),
                   msg_details,
                   hidden_button_label ? "visible" : "hidden",
                   hidden_button_accesskey, hidden_button_label,
                   button_accesskey, button_label);
#pragma GCC diagnostic pop

  /* Make our history backend ignore the next page load, since it will be an error page. */
  ephy_web_view_freeze_history (view);
  webkit_web_view_load_alternate_html (WEBKIT_WEB_VIEW (view), html->str, uri, 0);
}

static gboolean
load_failed_cb (WebKitWebView   *web_view,
                WebKitLoadEvent  load_event,
                const char      *uri,
                GError          *error,
                gpointer         user_data)
{
  EphyWebView *view = EPHY_WEB_VIEW (web_view);

  view->load_failed = TRUE;
  ephy_web_view_set_link_message (view, NULL);

  if (error->domain != WEBKIT_NETWORK_ERROR &&
      error->domain != WEBKIT_POLICY_ERROR &&
      error->domain != WEBKIT_PLUGIN_ERROR) {
    if (view->address && g_str_has_prefix (view->address, "file:"))
      ephy_web_view_load_error_page (view, uri, EPHY_WEB_VIEW_ERROR_NO_SUCH_FILE, error, NULL);
    else
      ephy_web_view_load_error_page (view, uri, EPHY_WEB_VIEW_ERROR_PAGE_NETWORK_ERROR, error, NULL);
    return TRUE;
  }

  switch (error->code) {
    case WEBKIT_NETWORK_ERROR_FAILED:
    case WEBKIT_NETWORK_ERROR_TRANSPORT:
    case WEBKIT_NETWORK_ERROR_UNKNOWN_PROTOCOL:
    case WEBKIT_NETWORK_ERROR_FILE_DOES_NOT_EXIST:
    case WEBKIT_POLICY_ERROR_FAILED:
    case WEBKIT_POLICY_ERROR_CANNOT_SHOW_MIME_TYPE:
    case WEBKIT_POLICY_ERROR_CANNOT_SHOW_URI:
    case WEBKIT_POLICY_ERROR_CANNOT_USE_RESTRICTED_PORT:
    case WEBKIT_PLUGIN_ERROR_FAILED:
    case WEBKIT_PLUGIN_ERROR_CANNOT_FIND_PLUGIN:
    case WEBKIT_PLUGIN_ERROR_CANNOT_LOAD_PLUGIN:
    case WEBKIT_PLUGIN_ERROR_JAVA_UNAVAILABLE:
    case WEBKIT_PLUGIN_ERROR_CONNECTION_CANCELLED:
      ephy_web_view_load_error_page (view, uri, EPHY_WEB_VIEW_ERROR_PAGE_NETWORK_ERROR, error, NULL);
      return TRUE;
    case WEBKIT_NETWORK_ERROR_CANCELLED: {
      if (!view->typed_address) {
        const char *prev_uri;

        prev_uri = webkit_web_view_get_uri (web_view);
        ephy_web_view_set_address (view, prev_uri);
      }
    }
    break;
    case WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE:
      /* If we are going to download something, and this is the first
       * page to load in this tab, we may want to close it down. */
      if (!view->ever_committed)
        g_signal_emit_by_name (view, "download-only-load", NULL);
      break;
    /* In case the resource is going to be showed with a plugin just let
     * WebKit do it */
    case WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD:
    default:
      break;
  }

  return FALSE;
}

static gboolean
load_failed_with_tls_error_cb (WebKitWebView        *web_view,
                               const char           *uri,
                               GTlsCertificate      *certificate,
                               GTlsCertificateFlags  errors,
                               gpointer              user_data)
{
  EphyWebView *view = EPHY_WEB_VIEW (web_view);

  g_clear_object (&view->certificate);
  g_clear_pointer (&view->tls_error_failing_uri, g_free);

  view->certificate = g_object_ref (certificate);
  view->tls_errors = errors;
  view->tls_error_failing_uri = g_strdup (uri);
  ephy_web_view_load_error_page (EPHY_WEB_VIEW (web_view), uri,
                                 EPHY_WEB_VIEW_ERROR_INVALID_TLS_CERTIFICATE, NULL, NULL);

  return TRUE;
}

static void
mixed_content_detected_cb (WebKitWebView              *web_view,
                           WebKitInsecureContentEvent  event,
                           gpointer                    user_data)
{
  EphyWebView *view = EPHY_WEB_VIEW (web_view);

  if (view->security_level != EPHY_SECURITY_LEVEL_UNACCEPTABLE_CERTIFICATE)
    ephy_web_view_set_security_level (view, EPHY_SECURITY_LEVEL_MIXED_CONTENT);
}

static void
close_web_view_cb (WebKitWebView *web_view,
                   gpointer       user_data)

{
  GtkWidget *widget = gtk_widget_get_toplevel (GTK_WIDGET (web_view));

  LOG ("close web view");

  if (EPHY_IS_EMBED_CONTAINER (widget))
    ephy_embed_container_remove_child (EPHY_EMBED_CONTAINER (widget),
                                       EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (web_view));
  else
    gtk_widget_destroy (widget);
}


static void
zoom_changed_cb (WebKitWebView *web_view,
                 GParamSpec    *pspec,
                 gpointer       user_data)
{
  const char *address;
  double zoom;

  zoom = webkit_web_view_get_zoom_level (web_view);

  if (EPHY_WEB_VIEW (web_view)->is_setting_zoom)
    return;

  address = ephy_web_view_get_address (EPHY_WEB_VIEW (web_view));
  if (ephy_embed_utils_address_has_web_scheme (address)) {
    ephy_history_service_set_url_zoom_level (EPHY_WEB_VIEW (web_view)->history_service,
                                             address, zoom,
                                             NULL, NULL, NULL);
  }
}

static gboolean
script_dialog_cb (WebKitWebView      *web_view,
                  WebKitScriptDialog *dialog)
{
  if (webkit_script_dialog_get_dialog_type (dialog) != WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM)
    return FALSE;

  /* Ignore beforeunload events for now until we properly support webkit_web_view_try_close()
   * See https://bugzilla.gnome.org/show_bug.cgi?id=722032.
   */
  webkit_script_dialog_confirm_set_confirmed (dialog, TRUE);

  return TRUE;
}

static const char *
enum_nick (GType enum_type,
           int   value)
{
  GEnumClass *enum_class;
  const GEnumValue *enum_value;
  const char *nick = NULL;

  enum_class = g_type_class_ref (enum_type);
  enum_value = g_enum_get_value (enum_class, value);
  if (enum_value)
    nick = enum_value->value_nick;

  g_type_class_unref (enum_class);
  return nick;
}

static void
reader_setting_changed_cb (EphyWebView *web_view)
{
  HdyStyleManager *style_manager;
  const gchar *font_style;
  const gchar *color_scheme;
  gchar *js_snippet;

  if (!g_str_has_prefix (web_view->address, EPHY_READER_SCHEME))
    return;

  font_style = enum_nick (EPHY_TYPE_PREFS_READER_FONT_STYLE,
                          g_settings_get_enum (EPHY_SETTINGS_READER,
                                               EPHY_PREFS_READER_FONT_STYLE));

  style_manager = hdy_style_manager_get_default ();

  if (hdy_style_manager_get_system_supports_color_schemes (style_manager))
    color_scheme = hdy_style_manager_get_dark (style_manager) ? "dark" : "light";
  else
    color_scheme = enum_nick (EPHY_TYPE_PREFS_READER_COLOR_SCHEME,
                              g_settings_get_enum (EPHY_SETTINGS_READER,
                                                   EPHY_PREFS_READER_COLOR_SCHEME));

  js_snippet = g_strdup_printf ("document.body.className = '%s %s'",
                                font_style,
                                color_scheme);
  webkit_web_view_run_javascript_in_world (WEBKIT_WEB_VIEW (web_view),
                                           js_snippet,
                                           ephy_embed_shell_get_guid (ephy_embed_shell_get_default ()),
                                           NULL,
                                           NULL,
                                           NULL);
  g_free (js_snippet);
}

typedef struct {
  EphyWebView *web_view;
  WebKitAuthenticationRequest *request;
} AuthenticationData;

static AuthenticationData *
authentication_data_new (EphyWebView                 *web_view,
                         WebKitAuthenticationRequest *request)
{
  AuthenticationData *data;

  data = g_new (AuthenticationData, 1);
  data->web_view = g_object_ref (web_view);
  data->request = g_object_ref (request);

  return data;
}

static void
authentication_data_free (AuthenticationData *data)
{
  g_object_unref (data->web_view);
  g_object_unref (data->request);
  g_free (data);
}

static void
auth_password_query_finished_cb (GList              *records,
                                 AuthenticationData *data)
{
  EphyPasswordRecord *record;
  g_autoptr (WebKitCredential) credential = NULL;

  record = records && records->data ? EPHY_PASSWORD_RECORD (records->data) : NULL;
  if (record) {
    credential = webkit_credential_new (ephy_password_record_get_username (record),
                                        ephy_password_record_get_password (record),
                                        WEBKIT_CREDENTIAL_PERSISTENCE_NONE);
  } else {
    /* Provide a non-empty wrong credential to force a retry */
    credential = webkit_credential_new (" ", "", WEBKIT_CREDENTIAL_PERSISTENCE_NONE);
  }

  webkit_authentication_request_authenticate (data->request, credential);
  authentication_data_free (data);
}

static void
authenticate_succeeded_cb (WebKitAuthenticationRequest *request,
                           WebKitCredential            *credential)
{
  EphyPasswordManager *password_manager;
  g_autoptr (WebKitSecurityOrigin) security_origin = NULL;
  g_autofree char *origin = NULL;

  if (webkit_credential_get_persistence (credential) != WEBKIT_CREDENTIAL_PERSISTENCE_PERMANENT)
    return;

  security_origin = webkit_authentication_request_get_security_origin (request);
  origin = webkit_security_origin_to_string (security_origin);
  password_manager = ephy_embed_shell_get_password_manager (ephy_embed_shell_get_default ());
  ephy_password_manager_save (password_manager,
                              origin,
                              origin,
                              webkit_credential_get_username (credential),
                              webkit_credential_get_password (credential),
                              "org.gnome.Epiphany.HTTPAuthCredentials.Username",
                              "org.gnome.Epiphany.HTTPAuthCredentials.Password",
                              TRUE);
}

static gboolean
authenticate_cb (WebKitWebView               *web_view,
                 WebKitAuthenticationRequest *request,
                 gpointer                     user_data)
{
  EphyWebView *ephy_web_view = EPHY_WEB_VIEW (web_view);
  EphyPasswordManager *password_manager;
  AuthenticationData *data;
  g_autoptr (WebKitSecurityOrigin) security_origin = NULL;
  g_autofree char *origin = NULL;

  if (webkit_authentication_request_is_retry (request)) {
    webkit_authentication_request_set_can_save_credentials (request, TRUE);
    g_signal_connect_object (request, "authenticated",
                             G_CALLBACK (authenticate_succeeded_cb),
                             ephy_web_view, 0);
    ephy_web_view->in_auth_dialog = 1;
    return FALSE;
  }

  data = authentication_data_new (ephy_web_view, request);
  security_origin = webkit_authentication_request_get_security_origin (request);
  origin = webkit_security_origin_to_string (security_origin);
  password_manager = ephy_embed_shell_get_password_manager (ephy_embed_shell_get_default ());
  ephy_password_manager_query (password_manager,
                               NULL,
                               origin,
                               origin,
                               NULL,
                               "org.gnome.Epiphany.HTTPAuthCredentials.Username",
                               "org.gnome.Epiphany.HTTPAuthCredentials.Password",
                               (EphyPasswordManagerQueryCallback)auth_password_query_finished_cb,
                               data);
  return TRUE;
}

typedef struct {
  WebKitWebView *web_view;
  char *origin;
  WebKitUserMessage *message;
} PasswordManagerData;

static void
password_manager_data_free (PasswordManagerData *data)
{
  g_object_unref (data->web_view);
  g_object_unref (data->message);
  g_free (data);
}

static void
password_manager_query_finished_cb (GList               *records,
                                    PasswordManagerData *data)
{
  EphyPasswordRecord *record;
  const char *origin;
  const char *username = NULL;
  const char *password = NULL;
  g_autofree char *real_origin = NULL;

  record = records && records->data ? EPHY_PASSWORD_RECORD (records->data) : NULL;
  if (record) {
    username = ephy_password_record_get_username (record);
    password = ephy_password_record_get_password (record);
  }

  g_variant_get (webkit_user_message_get_parameters (data->message), "(&s@sm@sm@s@s)", &origin, NULL, NULL, NULL, NULL);
  real_origin = ephy_uri_to_security_origin (webkit_web_view_get_uri (data->web_view));
  if (g_strcmp0 (real_origin, origin) != 0) {
    g_debug ("Extension's origin '%s' doesn't match real origin '%s'", origin, real_origin);
    password_manager_data_free (data);
    return;
  }

  webkit_user_message_send_reply (data->message,
                                  webkit_user_message_new ("PasswordManager.QueryPasswordResponse",
                                                           g_variant_new ("(msms)", username, password)));
  password_manager_data_free (data);
}

static gboolean
password_manager_handle_query_usernames_message (WebKitWebView     *web_view,
                                                 WebKitUserMessage *message)
{
  GVariant *parameters;
  const char *origin;
  EphyPasswordManager *password_manager;
  GList *usernames, *l;
  GVariantBuilder builder;
  g_autofree char *real_origin = NULL;

  parameters = webkit_user_message_get_parameters (message);
  if (!parameters)
    return FALSE;

  g_variant_get (parameters, "&s", &origin);
  real_origin = ephy_uri_to_security_origin (webkit_web_view_get_uri (web_view));
  if (g_strcmp0 (real_origin, origin) != 0) {
    g_debug ("Extension's origin '%s' doesn't match real origin '%s'", origin, real_origin);
    return FALSE;
  }

  password_manager = ephy_embed_shell_get_password_manager (ephy_embed_shell_get_default ());
  usernames = ephy_password_manager_get_usernames_for_origin (password_manager, origin);

  g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);
  for (l = usernames; l != NULL; l = g_list_next (l))
    g_variant_builder_add (&builder, "s", l->data);

  webkit_user_message_send_reply (message, webkit_user_message_new ("PasswordManager.QueryUsernamesResponse",
                                                                    g_variant_builder_end (&builder)));
  return TRUE;
}

static gboolean
password_manager_handle_query_password_message (WebKitWebView     *web_view,
                                                WebKitUserMessage *message)
{
  GVariant *parameters;
  const char *origin;
  const char *target_origin;
  const char *username;
  const char *username_field;
  const char *password_field;
  EphyPasswordManager *password_manager;
  PasswordManagerData *data;

  parameters = webkit_user_message_get_parameters (message);
  if (!parameters)
    return FALSE;

  g_variant_get (parameters, "(&s&sm&sm&s&s)", &origin, &target_origin, &username, &username_field, &password_field);

  /* Don't include username_field in queries unless we actually have a username
   * to go along with it, or the query will fail because we don't save
   * username_field without a corresponding username.
   */
  if (!username && username_field)
    username_field = NULL;

  data = g_new (PasswordManagerData, 1);
  data->web_view = g_object_ref (web_view);
  data->message = g_object_ref (message);

  password_manager = ephy_embed_shell_get_password_manager (ephy_embed_shell_get_default ());
  ephy_password_manager_query (password_manager,
                               NULL,
                               origin,
                               target_origin,
                               username,
                               username_field,
                               password_field,
                               (EphyPasswordManagerQueryCallback)password_manager_query_finished_cb,
                               data);
  return TRUE;
}

static gboolean
user_message_received_cb (WebKitWebView     *web_view,
                          WebKitUserMessage *message)
{
  const char *name;

  name = webkit_user_message_get_name (message);
  if (g_strcmp0 (name, "PasswordManager.QueryUsernames") == 0)
    return password_manager_handle_query_usernames_message (web_view, message);

  if (g_strcmp0 (name, "PasswordManager.QueryPassword") == 0)
    return password_manager_handle_query_password_message (web_view, message);

  return FALSE;
}

static void
scale_factor_changed_cb (EphyWebView *web_view,
                         GParamSpec  *pspec,
                         gpointer     user_data)
{
  _ephy_web_view_update_icon (web_view);
}

GtkWidget *
ephy_web_view_new_with_user_content_manager (WebKitUserContentManager *ucm)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();

  return g_object_new (EPHY_TYPE_WEB_VIEW,
                       "web-context", ephy_embed_shell_get_web_context (shell),
                       "user-content-manager", ucm,
                       "settings", ephy_embed_prefs_get_settings (),
                       "is-controlled-by-automation", ephy_embed_shell_get_mode (shell) == EPHY_EMBED_SHELL_MODE_AUTOMATION,
                       NULL);
}

/**
 * ephy_web_view_load_request:
 * @view: the #EphyWebView in which to load the request
 * @request: the #WebKitNetworkRequest to be loaded
 *
 * Loads the given #WebKitNetworkRequest in the given #EphyWebView.
 **/
void
ephy_web_view_load_request (EphyWebView      *view,
                            WebKitURIRequest *request)
{
  const char *url;
  char *effective_url;

  g_assert (EPHY_IS_WEB_VIEW (view));
  g_assert (WEBKIT_IS_URI_REQUEST (request));

  url = webkit_uri_request_get_uri (request);
  effective_url = ephy_embed_utils_normalize_address (url);

  webkit_uri_request_set_uri (request, effective_url);
  g_free (effective_url);

  webkit_web_view_load_request (WEBKIT_WEB_VIEW (view), request);
}

/**
 * ephy_web_view_load_url:
 * @view: an #EphyWebView
 * @url: a URL
 *
 * Loads @url in @view.
 **/
void
ephy_web_view_load_url (EphyWebView *view,
                        const char  *url)
{
  char *effective_url;

  g_assert (EPHY_IS_WEB_VIEW (view));
  g_assert (url);

  effective_url = ephy_embed_utils_normalize_address (url);
  if (g_str_has_prefix (effective_url, "javascript:")) {
    g_autoptr (GUri) uri = NULL;
    g_autofree char *decoded_url = NULL;

    uri = g_uri_parse (effective_url, G_URI_FLAGS_NONE, NULL);
    decoded_url = g_uri_to_string (uri);
    webkit_web_view_run_javascript (WEBKIT_WEB_VIEW (view), decoded_url, NULL, NULL, NULL);
  } else
    webkit_web_view_load_uri (WEBKIT_WEB_VIEW (view), effective_url);

  g_free (effective_url);
}

/**
 * ephy_web_view_get_is_blank:
 * @view: an #EphyWebView
 *
 * Returns whether the  @view's address is "blank".
 *
 * Return value: %TRUE if the @view's address is "blank"
 **/
gboolean
ephy_web_view_get_is_blank (EphyWebView *view)
{
  return view->is_blank;
}

gboolean
ephy_web_view_is_overview (EphyWebView *view)
{
  if (!view->address)
    return FALSE;

  return (!strcmp (view->address, EPHY_ABOUT_SCHEME ":overview") ||
          !strcmp (view->address, "about:overview"));
}

gboolean
ephy_web_view_is_newtab (EphyWebView *view)
{
  if (!view->address)
    return FALSE;

  return (!strcmp (view->address, EPHY_ABOUT_SCHEME ":newtab") ||
          !strcmp (view->address, "about:newtab"));
}

/**
 * ephy_web_view_get_address:
 * @view: an #EphyWebView
 *
 * Returns the address of the currently-loaded page, percent-encoded.
 * This URI should not be displayed to the user; to do that, use
 * ephy_web_view_get_display_address().
 *
 * Return value: @view's address. Will never be %NULL.
 **/
const char *
ephy_web_view_get_address (EphyWebView *view)
{
  if (view->address) {
    if (g_str_has_prefix (view->address, EPHY_READER_SCHEME))
      return view->address + strlen (EPHY_READER_SCHEME) + 1;

    return view->address;
  }

  return "about:blank";
}

/**
 * ephy_web_view_get_display_address:
 * @view: an #EphyWebView
 *
 * Returns the display address of the currently-loaded page. This is a
 * decoded URI suitable for display to the user. To get a URI suitable
 * for sending to a server, e.g. for storage in the bookmarks or history
 * database, use ephy_web_view_get_address().
 *
 * Return value: @view's address. Will never be %NULL.
 */
const char *
ephy_web_view_get_display_address (EphyWebView *view)
{
  return view->display_address ? view->display_address : "about:blank";
}

/**
 * ephy_web_view_is_loading:
 * @view: an #EphyWebView
 *
 * Returns whether the web page in @view has finished loading. A web
 * page is only finished loading after all images, styles, and other
 * dependencies have been downloaded and rendered, or when the load
 * has failed for some reason.
 *
 * Return value: %TRUE if the page is still loading, %FALSE if complete
 **/
gboolean
ephy_web_view_is_loading (EphyWebView *view)
{
  return webkit_web_view_is_loading (WEBKIT_WEB_VIEW (view));
}

/**
 * ephy_web_view_load_failed:
 * @view: an #EphyWebView
 *
 * Returns whether the web page in @view has failed to load.
 *
 * Return value: %TRUE if the page failed to load, %FALSE if it's loading
 * or load finished successfully
 **/
gboolean
ephy_web_view_load_failed (EphyWebView *view)
{
  return view->load_failed;
}

/**
 * ephy_web_view_get_icon:
 * @view: an #EphyWebView
 *
 * Returns the view's site icon as a #GdkPixbuf,
 * or %NULL if it is not available.
 *
 * Return value: (transfer none): a the view's site icon
 **/
GdkPixbuf *
ephy_web_view_get_icon (EphyWebView *view)
{
  return view->icon;
}

/**
 * ephy_web_view_get_document_type:
 * @view: an #EphyWebView
 *
 * Returns the type of document loaded in the @view
 *
 * Return value: the #EphyWebViewDocumentType
 **/
EphyWebViewDocumentType
ephy_web_view_get_document_type (EphyWebView *view)
{
  return view->document_type;
}

/**
 * ephy_web_view_get_navigation_flags:
 * @view: an #EphyWebView
 *
 * Returns @view's navigation flags.
 *
 * Return value: @view's navigation flags
 **/
EphyWebViewNavigationFlags
ephy_web_view_get_navigation_flags (EphyWebView *view)
{
  return view->nav_flags;
}

/**
 * ephy_web_view_get_status_message:
 * @view: an #EphyWebView
 *
 * Returns the message displayed in @view's #EphyWindow's
 * #EphyStatusbar. If the user is hovering the mouse over a hyperlink,
 * this function will return the same value as
 * ephy_web_view_get_link_message(). Otherwise, it will return a network
 * status message, or NULL.
 *
 * The message returned has a limited lifetime, and so should be copied with
 * g_strdup() if it must be stored.
 *
 * Return value: The current statusbar message
 **/
const char *
ephy_web_view_get_status_message (EphyWebView *view)
{
  g_assert (EPHY_IS_WEB_VIEW (view));

  if (view->link_message && view->link_message[0] != '\0')
    return view->link_message;

  if (view->loading_message)
    return view->loading_message;

  return NULL;
}

/**
 * ephy_web_view_get_link_message:
 * @view: an #EphyWebView
 *
 * When the user is hovering the mouse over a hyperlink, returns the URL of the
 * hyperlink.
 *
 * Return value: the URL of the link over which the mouse is hovering
 **/
const char *
ephy_web_view_get_link_message (EphyWebView *view)
{
  g_assert (EPHY_IS_WEB_VIEW (view));

  return view->link_message;
}

/**
 * ephy_web_view_set_link_message:
 * @view: an #EphyWebView
 * @address: new value for link-message in @view
 *
 * Sets the value of link-message property which tells the URL of the hovered
 * link.
 **/
void
ephy_web_view_set_link_message (EphyWebView *view,
                                const char  *address)
{
  char *decoded_address;

  g_assert (EPHY_IS_WEB_VIEW (view));

  g_free (view->link_message);

  if (address) {
    decoded_address = ephy_uri_decode (address);
    view->link_message = ephy_embed_utils_link_message_parse (decoded_address);
    g_free (decoded_address);
  } else {
    view->link_message = NULL;
  }

  g_object_notify_by_pspec (G_OBJECT (view), obj_properties[PROP_STATUS_MESSAGE]);
  g_object_notify_by_pspec (G_OBJECT (view), obj_properties[PROP_LINK_MESSAGE]);
}

/**
 * ephy_web_view_set_security_level:
 * @view: an #EphyWebView
 * @level: the new #EphySecurityLevel for @view
 *
 * Sets @view's security-level property to @level.
 **/
void
ephy_web_view_set_security_level (EphyWebView       *view,
                                  EphySecurityLevel  level)
{
  g_assert (EPHY_IS_WEB_VIEW (view));

  if (view->security_level != level) {
    view->security_level = level;

    g_object_notify_by_pspec (G_OBJECT (view), obj_properties[PROP_SECURITY]);
  }
}

/**
 * ephy_web_view_get_typed_address:
 * @view: an #EphyWebView
 *
 * Returns the text that the user introduced in the @view's
 * #EphyWindow location entry, if any.
 *
 * This is not guaranteed to be the same as @view's location,
 * available through ephy_web_view_get_address(). As the user types a
 * new address into the location entry,
 * ephy_web_view_get_typed_address()'s returned string will
 * change. When the load starts, ephy_web_view_get_typed_address()
 * will return %NULL, and ephy_web_view_get_address() will return the
 * new page being loaded. Note that the typed_address can be changed
 * again while a load is in progress (in case the user starts to type
 * again in the location entry); in that case
 * ephy_web_view_get_typed_address() will be again non-%NULL, and the
 * contents of the entry will not be overwritten.
 *
 * Return value: @view's #EphyWindow's location entry text when @view
 * is selected.
 **/
const char *
ephy_web_view_get_typed_address (EphyWebView *view)
{
  g_assert (EPHY_IS_WEB_VIEW (view));

  return view->typed_address;
}

/**
 * ephy_web_view_set_typed_address:
 * @view: an #EphyWebView
 * @address: the new typed address, or %NULL to clear it
 *
 * Sets the text that @view's #EphyWindow will display in its location toolbar
 * entry when @view is selected.
 **/
void
ephy_web_view_set_typed_address (EphyWebView *view,
                                 const char  *address)
{
  g_assert (EPHY_IS_WEB_VIEW (view));

  g_free (view->typed_address);
  view->typed_address = g_strdup (address);

  g_object_notify_by_pspec (G_OBJECT (view), obj_properties[PROP_TYPED_ADDRESS]);
}

gboolean
ephy_web_view_get_should_bypass_safe_browsing (EphyWebView *view)
{
  g_assert (EPHY_IS_WEB_VIEW (view));

  return view->bypass_safe_browsing;
}

void
ephy_web_view_set_should_bypass_safe_browsing (EphyWebView *view,
                                               gboolean     bypass_safe_browsing)
{
  g_assert (EPHY_IS_WEB_VIEW (view));

  view->bypass_safe_browsing = bypass_safe_browsing;
}

static gboolean
has_modified_forms_timeout_cb (gpointer user_data)
{
  GTask *task = user_data;

  g_assert (!g_task_get_completed (task));
  g_task_set_task_data (task, GINT_TO_POINTER (0), NULL);
  g_task_return_boolean (task, FALSE);

  return G_SOURCE_REMOVE;
}

static void
has_modified_forms_cb (WebKitWebView *view,
                       GAsyncResult  *result,
                       GTask         *task)
{
  WebKitJavascriptResult *js_result;
  gboolean retval = FALSE;
  GError *error = NULL;
  gulong id;

  js_result = webkit_web_view_run_javascript_in_world_finish (view, result, &error);

  id = GPOINTER_TO_INT (g_task_get_task_data (task));
  if (id == 0) {
    /* We hit the timeout. Our task has already returned. */
    goto out;
  }
  g_source_remove (id);

  if (!js_result) {
    g_task_return_error (task, error);
  } else {
    retval = jsc_value_to_boolean (webkit_javascript_result_get_js_value (js_result));
    g_task_return_boolean (task, retval);
  }

out:
  if (js_result)
    webkit_javascript_result_unref (js_result);
  g_object_unref (task);
}

/**
 * ephy_web_view_has_modified_forms:
 * @view: an #EphyWebView
 *
 * A small heuristic is used here. If there's only one input element modified
 * and it does not have a lot of text the user is likely not very interested in
 * saving this work, so it returns %FALSE in this case (eg, google search
 * input).
 *
 * Returns %TRUE if the user has modified &lt;input&gt; or &lt;textarea&gt;
 * values in @view's loaded document.
 *
 * Return value: %TRUE if @view has user-modified forms
 **/
void
ephy_web_view_has_modified_forms (EphyWebView         *view,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GTask *task;
  gulong id;

  g_assert (EPHY_IS_WEB_VIEW (view));

  task = g_task_new (view, cancellable, callback, user_data);

  /* Set timeout to guard against web process hangs. Otherwise, a single
   * unresponsive web process would prevent the window from closing. Note that
   * although webkit_web_view_run_javascript_in_world() takes a cancellable,
   * it's not *really* cancellable and attempting to cancel it just causes it to
   * return G_IO_ERROR_CANCELLED after however long it takes to finish, which
   * will be never if the web process is unresponsive, so we always fake
   * completion after a two second delay.
   */
  id = g_timeout_add_seconds (2, has_modified_forms_timeout_cb, task);
  g_task_set_task_data (task, GINT_TO_POINTER (id), NULL);

  webkit_web_view_run_javascript_in_world (WEBKIT_WEB_VIEW (view),
                                           "Ephy.hasModifiedForms();",
                                           ephy_embed_shell_get_guid (ephy_embed_shell_get_default ()),
                                           cancellable,
                                           (GAsyncReadyCallback)has_modified_forms_cb,
                                           task);
}

gboolean
ephy_web_view_has_modified_forms_finish (EphyWebView   *view,
                                         GAsyncResult  *result,
                                         GError       **error)
{
  g_assert (g_task_is_valid (result, view));

  return g_task_propagate_boolean (G_TASK (result), error);
}

typedef struct {
  char *icon_uri;
  char *icon_color;
} GetBestWebAppIconAsyncData;

static void
get_best_web_app_icon_async_data_free (GetBestWebAppIconAsyncData *data)
{
  g_free (data->icon_uri);
  g_free (data->icon_color);

  g_free (data);
}

static void
get_best_web_app_icon_cb (WebKitWebView *view,
                          GAsyncResult  *result,
                          GTask         *task)
{
  WebKitJavascriptResult *js_result;
  GError *error = NULL;

  js_result = webkit_web_view_run_javascript_in_world_finish (view, result, &error);
  if (js_result) {
    JSCValue *js_value, *js_uri, *js_color;
    GetBestWebAppIconAsyncData *data;

    data = g_new0 (GetBestWebAppIconAsyncData, 1);
    js_value = webkit_javascript_result_get_js_value (js_result);
    g_assert (jsc_value_is_object (js_value));

    js_uri = jsc_value_object_get_property (js_value, "url");
    data->icon_uri = jsc_value_to_string (js_uri);
    g_object_unref (js_uri);

    js_color = jsc_value_object_get_property (js_value, "icon");
    data->icon_color = jsc_value_is_null (js_color) || jsc_value_is_undefined (js_color) ? NULL : jsc_value_to_string (js_color);
    g_object_unref (js_color);

    g_task_return_pointer (task, data, (GDestroyNotify)get_best_web_app_icon_async_data_free);
    webkit_javascript_result_unref (js_result);
  } else
    g_task_return_error (task, error);

  g_object_unref (task);
}

void
ephy_web_view_get_best_web_app_icon (EphyWebView         *view,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  WebKitWebView *wk_view;
  GTask *task;
  char *script;

  g_assert (EPHY_IS_WEB_VIEW (view));
  wk_view = WEBKIT_WEB_VIEW (view);

  task = g_task_new (view, cancellable, callback, user_data);
  script = g_strdup_printf ("Ephy.getWebAppIcon(\"%s\");", webkit_web_view_get_uri (wk_view));
  webkit_web_view_run_javascript_in_world (wk_view,
                                           script,
                                           ephy_embed_shell_get_guid (ephy_embed_shell_get_default ()),
                                           cancellable,
                                           (GAsyncReadyCallback)get_best_web_app_icon_cb,
                                           task);
  g_free (script);
}

gboolean
ephy_web_view_get_best_web_app_icon_finish (EphyWebView   *view,
                                            GAsyncResult  *result,
                                            char         **icon_uri,
                                            GdkRGBA       *icon_color,
                                            GError       **error)
{
  GetBestWebAppIconAsyncData *data;
  GTask *task = G_TASK (result);

  g_assert (g_task_is_valid (result, view));

  data = g_task_propagate_pointer (task, error);
  if (!data)
    return FALSE;

  if (data->icon_uri != NULL && data->icon_uri[0] != '\0') {
    *icon_uri = data->icon_uri;
    data->icon_uri = NULL;
  }

  if (data->icon_color != NULL && data->icon_color[0] != '\0')
    gdk_rgba_parse (icon_color, data->icon_color);

  get_best_web_app_icon_async_data_free (data);

  return TRUE;
}

static void
get_web_app_title_cb (WebKitWebView *view,
                      GAsyncResult  *result,
                      GTask         *task)
{
  WebKitJavascriptResult *js_result;
  GError *error = NULL;

  js_result = webkit_web_view_run_javascript_in_world_finish (view, result, &error);
  if (js_result) {
    JSCValue *js_value;
    char *retval = NULL;

    js_value = webkit_javascript_result_get_js_value (js_result);
    if (!jsc_value_is_null (js_value) && !jsc_value_is_undefined (js_value))
      retval = jsc_value_to_string (js_value);
    g_task_return_pointer (task, retval, (GDestroyNotify)g_free);
    webkit_javascript_result_unref (js_result);
  } else
    g_task_return_error (task, error);

  g_object_unref (task);
}

void
ephy_web_view_get_web_app_title (EphyWebView         *view,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  GTask *task;

  g_assert (EPHY_IS_WEB_VIEW (view));

  task = g_task_new (view, cancellable, callback, user_data);
  webkit_web_view_run_javascript_in_world (WEBKIT_WEB_VIEW (view),
                                           "Ephy.getWebAppTitle();",
                                           ephy_embed_shell_get_guid (ephy_embed_shell_get_default ()),
                                           cancellable,
                                           (GAsyncReadyCallback)get_web_app_title_cb,
                                           task);
}

char *
ephy_web_view_get_web_app_title_finish (EphyWebView   *view,
                                        GAsyncResult  *result,
                                        GError       **error)
{
  g_assert (g_task_is_valid (result, view));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
get_web_app_mobile_capable_cb (WebKitWebView *view,
                               GAsyncResult  *result,
                               GTask         *task)
{
  WebKitJavascriptResult *js_result;
  GError *error = NULL;

  js_result = webkit_web_view_run_javascript_in_world_finish (view, result, &error);
  if (js_result) {
    JSCValue *js_value;
    gboolean retval = FALSE;

    js_value = webkit_javascript_result_get_js_value (js_result);
    retval = jsc_value_to_boolean (js_value);

    g_task_return_boolean (task, retval);
    webkit_javascript_result_unref (js_result);
  } else
    g_task_return_error (task, error);

  g_object_unref (task);
}

void
ephy_web_view_get_web_app_mobile_capable (EphyWebView         *view,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  GTask *task;

  g_assert (EPHY_IS_WEB_VIEW (view));

  task = g_task_new (view, cancellable, callback, user_data);
  webkit_web_view_run_javascript_in_world (WEBKIT_WEB_VIEW (view),
                                           "Ephy.getAppleMobileWebAppCapable();",
                                           ephy_embed_shell_get_guid (ephy_embed_shell_get_default ()),
                                           cancellable,
                                           (GAsyncReadyCallback)get_web_app_mobile_capable_cb,
                                           task);
}

gboolean
ephy_web_view_get_web_app_mobile_capable_finish (EphyWebView   *view,
                                                 GAsyncResult  *result,
                                                 GError       **error)
{
  g_assert (g_task_is_valid (result, view));

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * ephy_web_view_get_security_level:
 * @view: an #EphyWebView
 * @level: (out): return value of security level
 * @address: (out) (transfer none): the URI to which the security level corresponds
 * @certificate: (out) (transfer none): return value of TLS certificate
 * @errors: (out): return value of TLS errors
 *
 * Fetches the #EphySecurityLevel and a #GTlsCertificate associated
 * with @view and a #GTlsCertificateFlags showing what problems, if any,
 * have been found with that certificate.
 **/
void
ephy_web_view_get_security_level (EphyWebView           *view,
                                  EphySecurityLevel     *level,
                                  const char           **address,
                                  GTlsCertificate      **certificate,
                                  GTlsCertificateFlags  *errors)
{
  g_assert (EPHY_IS_WEB_VIEW (view));

  if (level)
    *level = view->security_level;

  if (address)
    *address = view->last_committed_address;

  if (certificate)
    *certificate = view->certificate;

  if (errors)
    *errors = view->tls_errors;
}

static void
ephy_web_view_print_failed (EphyWebView *view,
                            GError      *error)
{
  GtkWidget *info_bar;
  GtkWidget *label;
  GtkContainer *content_area;
  EphyEmbed *embed = EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view);

  info_bar = gtk_info_bar_new_with_buttons (_("_OK"), GTK_RESPONSE_OK, NULL);
  label = gtk_label_new (error->message);
  content_area = GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (info_bar)));

  gtk_info_bar_set_message_type (GTK_INFO_BAR (info_bar), GTK_MESSAGE_ERROR);
  gtk_container_add (content_area, label);
  g_signal_connect (info_bar, "response",
                    G_CALLBACK (gtk_widget_destroy), NULL);

  ephy_embed_add_top_widget (embed, info_bar, EPHY_EMBED_TOP_WIDGET_POLICY_RETAIN_ON_TRANSITION);
  gtk_widget_show_all (info_bar);
}

static void
print_operation_finished_cb (WebKitPrintOperation *operation,
                             EphyWebView          *view)
{
  ephy_embed_shell_set_page_setup (ephy_embed_shell_get_default (),
                                   webkit_print_operation_get_page_setup (operation));
}

static void
print_operation_failed_cb (WebKitPrintOperation *operation,
                           GError               *error,
                           EphyWebView          *view)
{
  g_signal_handlers_disconnect_by_func (operation, print_operation_finished_cb, view);
  ephy_web_view_print_failed (view, error);
}

/**
 * ephy_web_view_print:
 * @view: an #EphyWebView
 *
 * Opens a dialog to print the specified view.
 *
 * Since: 2.30
 **/
void
ephy_web_view_print (EphyWebView *view)
{
  WebKitPrintOperation *operation;
  EphyEmbedShell *shell;
  GtkPrintSettings *settings;

  g_assert (EPHY_IS_WEB_VIEW (view));

  shell = ephy_embed_shell_get_default ();

  operation = webkit_print_operation_new (WEBKIT_WEB_VIEW (view));
  g_signal_connect (operation, "finished",
                    G_CALLBACK (print_operation_finished_cb),
                    view);
  g_signal_connect (operation, "failed",
                    G_CALLBACK (print_operation_failed_cb),
                    view);
  webkit_print_operation_set_page_setup (operation, ephy_embed_shell_get_page_setup (shell));
  settings = ephy_embed_shell_get_print_settings (shell);
  gtk_print_settings_set (settings,
                          GTK_PRINT_SETTINGS_OUTPUT_BASENAME,
                          webkit_web_view_get_title (WEBKIT_WEB_VIEW (view)));
  webkit_print_operation_set_print_settings (operation, settings);
  if (webkit_print_operation_run_dialog (operation, NULL) == WEBKIT_PRINT_OPERATION_RESPONSE_PRINT)
    ephy_embed_shell_set_print_settings (shell, webkit_print_operation_get_print_settings (operation));

  g_object_unref (operation);
}

static void
web_resource_get_data_cb (WebKitWebResource *resource,
                          GAsyncResult      *result,
                          GOutputStream     *output_stream)
{
  guchar *data;
  gsize data_length;
  GInputStream *input_stream;
  GError *error = NULL;

  data = webkit_web_resource_get_data_finish (resource, result, &data_length, &error);
  if (!data) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Failed to save page: %s", error->message);
    g_error_free (error);
    g_object_unref (output_stream);
    return;
  }

  input_stream = g_memory_input_stream_new_from_data (data, data_length, g_free);
  g_output_stream_splice_async (output_stream, input_stream,
                                G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                G_PRIORITY_DEFAULT,
                                NULL, NULL, NULL);
  g_object_unref (input_stream);
  g_object_unref (output_stream);
}

static void
ephy_web_view_save_main_resource_cb (GFile         *file,
                                     GAsyncResult  *result,
                                     WebKitWebView *view)
{
  GFileOutputStream *output_stream;
  WebKitWebResource *resource;
  GError *error = NULL;

  output_stream = g_file_replace_finish (file, result, &error);
  if (!output_stream) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Failed to save page: %s", error->message);
    g_error_free (error);
    return;
  }

  resource = webkit_web_view_get_main_resource (view);
  webkit_web_resource_get_data (resource,
                                EPHY_WEB_VIEW (view)->cancellable,
                                (GAsyncReadyCallback)web_resource_get_data_cb,
                                output_stream);
}
/**
 * ephy_web_view_save:
 * @view: an #EphyWebView
 * @uri: location to store the saved page
 *
 * Saves the currently loaded page of @view to @uri.
 **/
void
ephy_web_view_save (EphyWebView *view,
                    const char  *uri)
{
  GFile *file;

  g_assert (EPHY_IS_WEB_VIEW (view));
  g_assert (uri);

  file = g_file_new_for_uri (uri);

  if (g_str_has_suffix (uri, ".mhtml"))
    webkit_web_view_save_to_file (WEBKIT_WEB_VIEW (view), file, WEBKIT_SAVE_MODE_MHTML,
                                  NULL, NULL, NULL);
  else
    g_file_replace_async (file, NULL, FALSE,
                          G_FILE_CREATE_REPLACE_DESTINATION | G_FILE_CREATE_PRIVATE,
                          G_PRIORITY_DEFAULT,
                          view->cancellable,
                          (GAsyncReadyCallback)ephy_web_view_save_main_resource_cb,
                          view);
  g_object_unref (file);
}

/**
 * ephy_web_view_load_homepage:
 * @view: an #EphyWebView
 *
 * Loads the homepage set by the user in @view.
 **/
void
ephy_web_view_load_homepage (EphyWebView *view)
{
  EphyEmbedShell *shell;
  EphyEmbedShellMode mode;
  char *home;

  g_assert (EPHY_IS_WEB_VIEW (view));

  shell = ephy_embed_shell_get_default ();
  mode = ephy_embed_shell_get_mode (shell);

  if (mode == EPHY_EMBED_SHELL_MODE_INCOGNITO ||
      mode == EPHY_EMBED_SHELL_MODE_AUTOMATION) {
    ephy_web_view_load_new_tab_page (view);
    return;
  }

  home = g_settings_get_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_HOMEPAGE_URL);
  if (home == NULL || home[0] == '\0') {
    ephy_web_view_load_new_tab_page (view);
  } else {
    ephy_web_view_freeze_history (view);
    ephy_web_view_set_visit_type (view, EPHY_PAGE_VISIT_HOMEPAGE);
    ephy_web_view_load_url (view, home);
  }
  g_free (home);
}

void
ephy_web_view_load_new_tab_page (EphyWebView *view)
{
  EphyEmbedShell *shell;
  EphyEmbedShellMode mode;

  g_assert (EPHY_IS_WEB_VIEW (view));

  shell = ephy_embed_shell_get_default ();
  mode = ephy_embed_shell_get_mode (shell);

  ephy_web_view_freeze_history (view);
  ephy_web_view_set_visit_type (view, EPHY_PAGE_VISIT_HOMEPAGE);
  if (mode == EPHY_EMBED_SHELL_MODE_INCOGNITO)
    ephy_web_view_load_url (view, "about:incognito");
  else if (mode == EPHY_EMBED_SHELL_MODE_AUTOMATION)
    ephy_web_view_load_url (view, "about:blank");
  else
    ephy_web_view_load_url (view, "about:overview");
}

/**
 * ephy_web_view_get_visit_type:
 * @view: an #EphyWebView
 *
 * Returns: the @view #EphyWebViewVisitType
 **/
EphyHistoryPageVisitType
ephy_web_view_get_visit_type (EphyWebView *view)
{
  g_assert (EPHY_IS_WEB_VIEW (view));

  return view->visit_type;
}

/**
 * ephy_web_view_set_visit_type:
 * @view: an #EphyWebView
 * @visit_type: an #EphyHistoryPageVisitType
 *
 * Sets the @visit_type for @view, so that the URI can be
 * properly weighted in the history backend.
 **/
void
ephy_web_view_set_visit_type (EphyWebView              *view,
                              EphyHistoryPageVisitType  visit_type)
{
  g_assert (EPHY_IS_WEB_VIEW (view));

  view->visit_type = visit_type;
}


/**
 * ephy_web_view_toggle_reader_mode:
 * @view: an #EphyWebView
 * @active: active flag
 *
 * Sets reader mode state to @active if necessary.
 **/
void
ephy_web_view_toggle_reader_mode (EphyWebView *view,
                                  gboolean     active)
{
  WebKitWebView *web_view = WEBKIT_WEB_VIEW (view);
  char *reader_uri = NULL;
  const gchar *address;
  gboolean view_active = g_str_has_prefix (view->address, EPHY_READER_SCHEME);

  if (view_active == active)
    return;

  address = ephy_web_view_get_address (view);

  if (view_active) {
    ephy_web_view_freeze_history (view);
    webkit_web_view_load_uri (web_view, address);
    return;
  }

  if (!ephy_web_view_is_reader_mode_available (view))
    return;

  reader_uri = g_strconcat (EPHY_READER_SCHEME, ":", address, NULL);

  view->entering_reader_mode = TRUE;
  g_object_notify_by_pspec (G_OBJECT (web_view), obj_properties[PROP_ENTERING_READER_MODE]);

  webkit_web_view_load_uri (web_view, reader_uri);
}

gboolean
ephy_web_view_is_reader_mode_available (EphyWebView *view)
{
  return view->reader_mode_available;
}

gboolean
ephy_web_view_get_reader_mode_state (EphyWebView *view)
{
  if (!view->address)
    return FALSE;
  return g_str_has_prefix (view->address, EPHY_READER_SCHEME);
}

gboolean
ephy_web_view_is_in_auth_dialog (EphyWebView *view)
{
  return view->in_auth_dialog;
}

static void
ephy_web_view_dispose (GObject *object)
{
  EphyWebView *view = EPHY_WEB_VIEW (object);
  WebKitUserContentManager *ucm = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (view));

  ephy_embed_prefs_unregister_ucm (ucm);
  ephy_embed_shell_unregister_ucm_handler (ephy_embed_shell_get_default (), ucm);

  untrack_info_bar (&view->geolocation_info_bar);
  untrack_info_bar (&view->notification_info_bar);
  untrack_info_bar (&view->microphone_info_bar);
  untrack_info_bar (&view->webcam_info_bar);
  untrack_info_bar (&view->webcam_mic_info_bar);
  untrack_info_bar (&view->password_info_bar);
  untrack_info_bar (&view->password_form_info_bar);
  untrack_info_bar (&view->itp_info_bar);

  g_clear_object (&view->certificate);
  g_clear_object (&view->file_monitor);
  g_clear_object (&view->icon);
  g_clear_pointer (&view->unresponsive_process_dialog, gtk_widget_destroy);

  if (view->cancellable) {
    g_cancellable_cancel (view->cancellable);
    g_clear_object (&view->cancellable);
  }

  g_clear_handle_id (&view->snapshot_timeout_id, g_source_remove);
  g_clear_handle_id (&view->reader_js_timeout, g_source_remove);
  g_clear_handle_id (&view->unresponsive_process_timeout_id, g_source_remove);

  G_OBJECT_CLASS (ephy_web_view_parent_class)->dispose (object);
}

static void
ephy_web_view_finalize (GObject *object)
{
  EphyWebView *view = EPHY_WEB_VIEW (object);

  g_free (view->address);
  g_free (view->display_address);
  g_free (view->typed_address);
  g_free (view->last_committed_address);
  g_free (view->link_message);
  g_free (view->loading_message);
  g_free (view->tls_error_failing_uri);
  g_free (view->pending_snapshot_uri);

  G_OBJECT_CLASS (ephy_web_view_parent_class)->finalize (object);
}

static void
ephy_web_view_constructed (GObject *object)
{
  EphyWebView *web_view = EPHY_WEB_VIEW (object);
  g_auto (GStrv) cors_allowlist = NULL;
  GtkStyleContext *context;
  GdkRGBA color;

  G_OBJECT_CLASS (ephy_web_view_parent_class)->constructed (object);

  g_signal_emit_by_name (ephy_embed_shell_get_default (), "web-view-created", web_view);

  g_signal_connect (web_view, "web-process-terminated",
                    G_CALLBACK (process_terminated_cb), NULL);
  g_signal_connect_swapped (webkit_web_view_get_back_forward_list (WEBKIT_WEB_VIEW (web_view)),
                            "changed", G_CALLBACK (update_navigation_flags), web_view);

  /* Avoid flashing a white background when loading the overview in
   * dark mode. Note that we have to later reset this to white before
   * loading any non-Epiphany page.
   */
  context = gtk_widget_get_style_context (GTK_WIDGET (web_view));
  if (gtk_style_context_lookup_color (context, "theme_base_color", &color))
    webkit_web_view_set_background_color (WEBKIT_WEB_VIEW (web_view), &color);

  cors_allowlist = g_new (char *, 2);
  cors_allowlist[0] = g_strdup ("ephy-resource://*/*");
  cors_allowlist[1] = NULL;
  webkit_web_view_set_cors_allowlist (WEBKIT_WEB_VIEW (web_view), (const char * const *)cors_allowlist);
}

static void
ephy_web_view_init (EphyWebView *web_view)
{
  EphyEmbedShell *shell;

  shell = ephy_embed_shell_get_default ();

  web_view->uid = web_view_uid++;

  web_view->is_blank = TRUE;
  web_view->ever_committed = FALSE;
  web_view->document_type = EPHY_WEB_VIEW_DOCUMENT_HTML;
  web_view->security_level = EPHY_SECURITY_LEVEL_TO_BE_DETERMINED;

  web_view->file_monitor = ephy_file_monitor_new (web_view);

  web_view->history_service = ephy_embed_shell_get_global_history_service (shell);

  web_view->cancellable = g_cancellable_new ();

  g_signal_connect_object (EPHY_SETTINGS_READER, "changed::" EPHY_PREFS_READER_FONT_STYLE,
                           G_CALLBACK (reader_setting_changed_cb),
                           web_view, G_CONNECT_SWAPPED);

  g_signal_connect_object (EPHY_SETTINGS_READER, "changed::" EPHY_PREFS_READER_COLOR_SCHEME,
                           G_CALLBACK (reader_setting_changed_cb),
                           web_view, G_CONNECT_SWAPPED);

  g_signal_connect_object (hdy_style_manager_get_default (),
                           "notify::system-supports-color-schemes",
                           G_CALLBACK (reader_setting_changed_cb),
                           web_view, G_CONNECT_SWAPPED);

  g_signal_connect_object (hdy_style_manager_get_default (),
                           "notify::dark",
                           G_CALLBACK (reader_setting_changed_cb),
                           web_view, G_CONNECT_SWAPPED);

  g_signal_connect (web_view, "decide-policy",
                    G_CALLBACK (decide_policy_cb),
                    NULL);

  g_signal_connect (web_view, "permission-request",
                    G_CALLBACK (permission_request_cb),
                    NULL);

  g_signal_connect (web_view, "load-changed",
                    G_CALLBACK (load_changed_cb),
                    NULL);

  g_signal_connect (web_view, "close",
                    G_CALLBACK (close_web_view_cb),
                    NULL);
  g_signal_connect (web_view, "load-failed",
                    G_CALLBACK (load_failed_cb),
                    NULL);

  g_signal_connect (web_view, "load-failed-with-tls-errors",
                    G_CALLBACK (load_failed_with_tls_error_cb),
                    NULL);

  g_signal_connect (web_view, "insecure-content-detected",
                    G_CALLBACK (mixed_content_detected_cb),
                    NULL);

  g_signal_connect (web_view, "notify::zoom-level",
                    G_CALLBACK (zoom_changed_cb),
                    NULL);

  g_signal_connect (web_view, "notify::title",
                    G_CALLBACK (title_changed_cb),
                    NULL);

  g_signal_connect (web_view, "notify::uri",
                    G_CALLBACK (uri_changed_cb),
                    NULL);

  g_signal_connect (web_view, "notify::is-web-process-responsive",
                    G_CALLBACK (is_web_process_responsive_changed_cb),
                    NULL);

  g_signal_connect (web_view, "mouse-target-changed",
                    G_CALLBACK (mouse_target_changed_cb),
                    NULL);

  g_signal_connect (web_view, "notify::favicon",
                    G_CALLBACK (icon_changed_cb),
                    NULL);

  g_signal_connect (web_view, "script-dialog",
                    G_CALLBACK (script_dialog_cb),
                    NULL);

  g_signal_connect (web_view, "authenticate",
                    G_CALLBACK (authenticate_cb),
                    NULL);

  g_signal_connect (web_view, "user-message-received",
                    G_CALLBACK (user_message_received_cb),
                    NULL);

  g_signal_connect (web_view, "notify::scale-factor",
                    G_CALLBACK (scale_factor_changed_cb),
                    NULL);

  g_signal_connect_object (shell, "password-form-focused",
                           G_CALLBACK (password_form_focused_cb),
                           web_view, 0);

  g_signal_connect_object (shell, "allow-tls-certificate",
                           G_CALLBACK (allow_tls_certificate_cb),
                           web_view, 0);

  g_signal_connect_object (shell, "allow-unsafe-browsing",
                           G_CALLBACK (allow_unsafe_browsing_cb),
                           web_view, 0);

  g_signal_connect_object (shell, "reload-page",
                           G_CALLBACK (reload_page_cb),
                           web_view, 0);
}

static void
ephy_web_view_class_init (EphyWebViewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  WebKitWebViewClass *webkit_webview_class = WEBKIT_WEB_VIEW_CLASS (klass);

  gobject_class->dispose = ephy_web_view_dispose;
  gobject_class->finalize = ephy_web_view_finalize;
  gobject_class->get_property = ephy_web_view_get_property;
  gobject_class->set_property = ephy_web_view_set_property;
  gobject_class->constructed = ephy_web_view_constructed;

  widget_class->button_press_event = ephy_web_view_button_press_event;

  webkit_webview_class->run_file_chooser = ephy_web_view_run_file_chooser;

/**
 * EphyWebView:address:
 *
 * View's current address. This is a percent-encoded URI.
 **/
  obj_properties[PROP_ADDRESS] =
    g_param_spec_string ("address",
                         "Address",
                         "The view's address",
                         "",
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

/**
 * EphyWebView:typed-address:
 *
 * User typed address for the current view.
 **/
  obj_properties[PROP_TYPED_ADDRESS] =
    g_param_spec_string ("typed-address",
                         "Typed Address",
                         "The typed address",
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

/**
 * EphyWebView:security-level:
 *
 * One of #EphySecurityLevel, determining view's current security level.
 **/
  obj_properties[PROP_SECURITY] =
    g_param_spec_enum ("security-level",
                       "Security Level",
                       "The view's security level",
                       EPHY_TYPE_SECURITY_LEVEL,
                       EPHY_SECURITY_LEVEL_TO_BE_DETERMINED,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

/**
 * EphyWebView:document-type:
 *
 * Document type determined for the view.
 **/
  obj_properties[PROP_DOCUMENT_TYPE] =
    g_param_spec_enum ("document-type",
                       "Document Type",
                       "The view's document type",
                       EPHY_TYPE_WEB_VIEW_DOCUMENT_TYPE,
                       EPHY_WEB_VIEW_DOCUMENT_HTML,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

/**
 * EphyWebView:navigation:
 *
 * View's navigation flags as #EphyWebViewNavigationFlags.
 **/
  obj_properties[PROP_NAVIGATION] =
    g_param_spec_flags ("navigation",
                        "Navigation flags",
                        "The view's navigation flags",
                        EPHY_TYPE_WEB_VIEW_NAVIGATION_FLAGS,
                        0,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

/**
 * EphyWebView:status-message:
 *
 * Statusbar message corresponding to this view.
 **/
  obj_properties[PROP_STATUS_MESSAGE] =
    g_param_spec_string ("status-message",
                         "Status Message",
                         "The view's statusbar message",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

/**
 * EphyWebView:link-message:
 *
 * ???
 **/
  obj_properties[PROP_LINK_MESSAGE] =
    g_param_spec_string ("link-message",
                         "Link Message",
                         "The view's link message",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

/**
 * EphyWebView:icon:
 *
 * View's favicon set by the loaded site.
 **/
  obj_properties[PROP_ICON] =
    g_param_spec_object ("icon",
                         "Icon",
                         "The view icon's",
                         GDK_TYPE_PIXBUF,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

/**
 * EphyWebView:is-blank:
 *
 * Whether the view is showing the blank address.
 **/
  obj_properties[PROP_IS_BLANK] =
    g_param_spec_boolean ("is-blank",
                          "Is blank",
                          "If the EphyWebView is blank",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

/**
 * EphyWebView:reader-mode:
 *
 * Whether the view is in reader mode.
 **/
  obj_properties[PROP_READER_MODE] =
    g_param_spec_boolean ("reader-mode",
                          "Reader mode",
                          "If the EphyWebView is in reader mode",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

/**
 * EphyWebView:display-address:
 *
 * View's current display address.
 **/
  obj_properties[PROP_DISPLAY_ADDRESS] =
    g_param_spec_string ("display-address",
                         "Display address",
                         "The view's display address",
                         "",
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

/**
 * EphyWebView:entering-reader-mode:
 *
 * Whether the view is entering reader mode.
 **/
  obj_properties[PROP_ENTERING_READER_MODE] =
    g_param_spec_boolean ("entering-reader-mode",
                          "Entering reader mode",
                          "If the EphyWebView is entering reader mode",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, LAST_PROP, obj_properties);

/**
 * EphyWebView::new-window:
 * @view: the #EphyWebView that received the signal
 * @new_view: the newly opened #EphyWebView
 *
 * The ::new-window signal is emitted after a new window has been opened by
 * the view. For example, when a JavaScript popup window is opened.
 **/
  g_signal_new ("new-window",
                EPHY_TYPE_WEB_VIEW,
                G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE,
                1,
                GTK_TYPE_WIDGET);

/**
 * EphyWebView::download-only-load:
 * @view: the #EphyWebView that received the signal
 *
 * The ::download-only-load signal is emitted when the @view has its main load
 * replaced by a download, and that is the only reason why the @view has been created.
 **/
  g_signal_new ("download-only-load",
                EPHY_TYPE_WEB_VIEW,
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE,
                0);
}

static void
ephy_web_view_ucm_add_custom_scripts (WebKitUserContentManager *ucm)
{
  g_autoptr (WebKitUserScript) script = NULL;
  g_autoptr (GBytes) youtube_js = NULL;
  g_auto (GStrv) allow_list = NULL;
  g_autoptr (GError) error = NULL;

  youtube_js = g_resources_lookup_data ("/org/gnome/epiphany/adguard/youtube.js", 0, &error);
  if (!youtube_js) {
    g_warning ("Failed to load youtube.js from AdGuard: %s", error->message);
    return;
  }

  allow_list = g_new (char *, 2);
  allow_list[0] = g_strdup ("https://*.youtube.com/*");
  allow_list[1] = NULL;

  script = webkit_user_script_new (g_bytes_get_data (youtube_js, NULL),
                                   WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
                                   WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END,
                                   (const char * const *)allow_list,
                                   NULL);
  webkit_user_content_manager_add_script (ucm, script);
}

/**
 * ephy_web_view_new:
 *
 * Equivalent to g_object_new() but returns an #GtkWidget so you don't have
 * to cast it when dealing with most code.
 *
 * Return value: the newly created #EphyWebView widget
 **/
GtkWidget *
ephy_web_view_new (void)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  g_autoptr (WebKitUserContentManager) ucm = webkit_user_content_manager_new ();

  ephy_embed_shell_register_ucm_handler (shell, ucm);
  ephy_embed_prefs_register_ucm (ucm);

  ephy_web_view_ucm_add_custom_scripts (ucm);

  return g_object_new (EPHY_TYPE_WEB_VIEW,
                       "web-context", ephy_embed_shell_get_web_context (shell),
                       "user-content-manager", ucm,
                       "settings", ephy_embed_prefs_get_settings (),
                       "is-controlled-by-automation", ephy_embed_shell_get_mode (shell) == EPHY_EMBED_SHELL_MODE_AUTOMATION,
                       NULL);
}

GtkWidget *
ephy_web_view_new_with_related_view (WebKitWebView *related_view)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  g_autoptr (WebKitUserContentManager) ucm = webkit_user_content_manager_new ();

  ephy_web_view_ucm_add_custom_scripts (ucm);

  ephy_embed_shell_register_ucm_handler (shell, ucm);
  ephy_embed_prefs_register_ucm (ucm);

  return g_object_new (EPHY_TYPE_WEB_VIEW,
                       "related-view", related_view,
                       "user-content-manager", ucm,
                       "settings", ephy_embed_prefs_get_settings (),
                       NULL);
}

guint64
ephy_web_view_get_uid (EphyWebView *web_view)
{
  return web_view->uid;
}
