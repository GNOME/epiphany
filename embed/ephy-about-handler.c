/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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
#include "ephy-about-handler.h"

#include "ephy-embed-shell.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-flatpak-utils.h"
#include "ephy-history-service.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-smaps.h"
#include "ephy-snapshot-service.h"
#include "ephy-web-app-utils.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

struct _EphyAboutHandler {
  GObject parent_instance;

  EphySMaps *smaps;
};

G_DEFINE_TYPE (EphyAboutHandler, ephy_about_handler, G_TYPE_OBJECT)


#define EPHY_ABOUT_OVERVIEW_MAX_ITEMS 9

#define EPHY_PAGE_TEMPLATE_ABOUT_CSS        "ephy-resource:///org/gnome/epiphany/page-templates/about.css"

static void
ephy_about_handler_finalize (GObject *object)
{
  EphyAboutHandler *handler = EPHY_ABOUT_HANDLER (object);

  g_clear_object (&handler->smaps);

  G_OBJECT_CLASS (ephy_about_handler_parent_class)->finalize (object);
}

static void
ephy_about_handler_init (EphyAboutHandler *handler)
{
}

static void
ephy_about_handler_class_init (EphyAboutHandlerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_about_handler_finalize;
}

static EphySMaps *
ephy_about_handler_get_smaps (EphyAboutHandler *handler)
{
  if (!handler->smaps)
    handler->smaps = ephy_smaps_new ();

  return handler->smaps;
}

static void
ephy_about_handler_finish_request (WebKitURISchemeRequest *request,
                                   gchar                  *data,
                                   gssize                  data_length)
{
  GInputStream *stream;

  data_length = data_length != -1 ? data_length : (gssize)strlen (data);
  stream = g_memory_input_stream_new_from_data (data, data_length, g_free);
  webkit_uri_scheme_request_finish (request, stream, data_length, "text/html");
  g_object_unref (stream);
}

static void
handle_memory_finished_cb (EphyAboutHandler       *handler,
                           GAsyncResult           *result,
                           WebKitURISchemeRequest *request)
{
  GString *data_str;
  gsize data_length;
  char *memory;

  data_str = g_string_new ("<html>");

  memory = g_task_propagate_pointer (G_TASK (result), NULL);
  if (memory) {
    g_string_append_printf (data_str, "<head><title>%s</title>"
                            "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
                            "<link href=\""EPHY_PAGE_TEMPLATE_ABOUT_CSS "\" rel=\"stylesheet\" type=\"text/css\">"
                            "</head><body>",
                            _("Memory usage"));

    g_string_append_printf (data_str, "<h1>%s</h1>", _("Memory usage"));
    g_string_append (data_str, memory);
    g_free (memory);
  }

  g_string_append (data_str, "</html>");

  data_length = data_str->len;
  ephy_about_handler_finish_request (request, g_string_free (data_str, FALSE), data_length);
  g_object_unref (request);
}

static void
handle_memory_sync (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  EphyAboutHandler *handler = EPHY_ABOUT_HANDLER (source_object);

  g_task_return_pointer (task,
                         ephy_smaps_to_html (ephy_about_handler_get_smaps (handler)),
                         g_free);
}

static gboolean
ephy_about_handler_handle_memory (EphyAboutHandler       *handler,
                                  WebKitURISchemeRequest *request)
{
  GTask *task;

  task = g_task_new (handler, NULL,
                     (GAsyncReadyCallback)handle_memory_finished_cb,
                     g_object_ref (request));
  g_task_run_in_thread (task, handle_memory_sync);
  g_object_unref (task);

  return TRUE;
}

static gboolean
ephy_about_handler_handle_about (EphyAboutHandler       *handler,
                                 WebKitURISchemeRequest *request)
{
  char *data;
  char *version;
  GtkIconInfo *icon_info;

  version = g_strdup_printf (_("Version %s"), VERSION);

  icon_info = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default (),
                                          APPLICATION_ID,
                                          256,
                                          GTK_ICON_LOOKUP_FORCE_SVG);

  data = g_strdup_printf ("<html><head><title>%s</title>"
                          "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
                          "<link href=\""EPHY_PAGE_TEMPLATE_ABOUT_CSS "\" rel=\"stylesheet\" type=\"text/css\">"
                          "</head><body>"
                          "<div class=\"dialog\">"
                          "<img src=\"file://%s\"/>"
                          "<h1 id=\"about-title\">%s</h1>"
                          "<h2 id=\"about-subtitle\">%s</h2>"
                          "<p id=\"about-tagline\">%s</p>"
                          "<table class=\"properties\">"
                          "<tr><td class=\"prop-label\">%s</td><td class=\"prop-value\">%d.%d.%d</td></tr>"
                          "</table>"
                          "</div></body></html>",
                          _("About Web"),
                          icon_info ? gtk_icon_info_get_filename (icon_info) : "",
#if !TECH_PREVIEW
                         _("Web"),
#else
                         _("Epiphany Technology Preview"),
#endif
                          version,
                          _("A simple, clean, beautiful view of the web"),
                          "WebKitGTK", webkit_get_major_version (), webkit_get_minor_version (), webkit_get_micro_version ());
  g_free (version);
  if (icon_info)
    g_object_unref (icon_info);

  ephy_about_handler_finish_request (request, data, -1);

  return TRUE;
}

static gboolean
ephy_about_handler_handle_epiphany (EphyAboutHandler       *handler,
                                    WebKitURISchemeRequest *request)
{
  char *data;

  data = g_strdup_printf ("<html class=\"epiphany-html\"><head><title>%s</title>"
                          "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
                          "<link href=\""EPHY_PAGE_TEMPLATE_ABOUT_CSS "\" rel=\"stylesheet\" type=\"text/css\">"
                          "</head><body class=\"epiphany-body\">"
                          "<div id=\"ephytext\">"
                          "“Il semble que la perfection soit atteinte non quand il n'y a plus rien à"
                          " ajouter, mais quand il n'y a plus rien à retrancher.”"
                          "</div>"
                          "<div id=\"from\">"
                          "<!-- Terre des Hommes, III: L'Avion, p. 60 -->"
                          "Antoine de Saint-Exupéry"
                          "</div></body></html>",
                          _("Web"));

  ephy_about_handler_finish_request (request, data, -1);

  return TRUE;
}

static void
handle_applications_finished_cb (EphyAboutHandler       *handler,
                                 GAsyncResult           *result,
                                 WebKitURISchemeRequest *request)
{
  GString *data_str;
  gsize data_length;
  GList *applications, *p;

  data_str = g_string_new (NULL);
  g_string_append_printf (data_str, "<html><head><title>%s</title>"
                          "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
                          "<link href=\""EPHY_PAGE_TEMPLATE_ABOUT_CSS "\" rel=\"stylesheet\" type=\"text/css\">"
                          "<script>"
                          "  function deleteWebApp(appID) {"
                          "    window.webkit.messageHandlers.aboutApps.postMessage(appID);"
                          "    var row = document.getElementById(appID);"
                          "    row.parentNode.removeChild(row);"
                          "  }"
                          "</script>"
                          "</head><body class=\"applications-body\"><h1>%s</h1>"
                          "<p>%s</p>",
                          _("Applications"),
                          _("Applications"),
                          _("List of installed web applications"));

  g_string_append (data_str, "<table>");

  applications = g_task_propagate_pointer (G_TASK (result), NULL);
  for (p = applications; p; p = p->next) {
    EphyWebApplication *app = (EphyWebApplication *)p->data;

    g_string_append_printf (data_str,
                            "<tbody><tr id =\"%s\">"
                            "<td class=\"icon\"><img width=64 height=64 src=\"file://%s\"></img></td>"
                            "<td class=\"data\"><div class=\"appname\">%s</div><div class=\"appurl\">%s</div></td>"
                            "<td class=\"input\"><input type=\"button\" value=\"%s\" onclick=\"deleteWebApp('%s');\"></td>"
                            "<td class=\"date\">%s <br /> %s</td></tr></tbody>",
                            app->id, app->icon_url, app->name, app->url, _("Delete"), app->id,
                            /* Note for translators: this refers to the installation date. */
                            _("Installed on:"), app->install_date);
  }

  g_string_append (data_str, "</table></body></html>");

  ephy_web_application_free_application_list (applications);

  data_length = data_str->len;
  ephy_about_handler_finish_request (request, g_string_free (data_str, FALSE), data_length);
  g_object_unref (request);
}

static void
handle_applications_sync (GTask        *task,
                          gpointer      source_object,
                          gpointer      task_data,
                          GCancellable *cancellable)
{
  g_task_return_pointer (task,
                         ephy_web_application_get_application_list (),
                         (GDestroyNotify)ephy_web_application_free_application_list);
}

static gboolean
ephy_about_handler_handle_applications (EphyAboutHandler       *handler,
                                        WebKitURISchemeRequest *request)
{
  GTask *task;

  task = g_task_new (handler, NULL,
                     (GAsyncReadyCallback)handle_applications_finished_cb,
                     g_object_ref (request));
  g_task_run_in_thread (task, handle_applications_sync);
  g_object_unref (task);

  return TRUE;
}

static void
history_service_query_urls_cb (EphyHistoryService     *history,
                               gboolean                success,
                               GList                  *urls,
                               WebKitURISchemeRequest *request)
{
  EphySnapshotService *snapshot_service;
  EphyEmbedShell *shell;
  GString *data_str;
  gsize data_length;
  char *lang;
  GList *l;

  snapshot_service = ephy_snapshot_service_get_default ();
  shell = ephy_embed_shell_get_default ();

  data_str = g_string_new (NULL);

  lang = g_strdup (pango_language_to_string (gtk_get_default_language ()));
  g_strdelimit (lang, "_-@", '\0');

  g_string_append_printf (data_str,
                          "<html xml:lang=\"%s\" lang=\"%s\" dir=\"%s\">\n"
                          "<head>\n"
                          "  <title>%s</title>\n"
                          "  <meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\" />\n"
                          "  <meta name=\"viewport\" content=\"width=device-width\">"
                          "  <link href=\""EPHY_PAGE_TEMPLATE_ABOUT_CSS "\" rel=\"stylesheet\" type=\"text/css\">\n"
                          "  <script> </script>\n"
                          "</head>\n"
                          "<body>\n",
                          lang, lang,
                          ((gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL) ? "rtl" : "ltr"),
                          _(OVERVIEW_PAGE_TITLE));
  g_free (lang);

  if (g_list_length (urls) == 0 || !success) {
    GtkIconInfo *icon_info;
    g_autofree gchar *icon = g_strconcat (APPLICATION_ID, "-symbolic", NULL);

    icon_info = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default (),
                                            icon,
                                            128,
                                            0);
    g_string_append_printf (data_str,
                            "  <div id=\"overview\" class=\"overview-empty\">\n"
                            "    <img src=\"file://%s\"/>\n"
                            "    <div><h1>%s</h1></div>\n"
                            "    <div><p>%s</p></div>\n"
                            "  </div>\n"
                            "</body></html>\n",
                            icon_info ? gtk_icon_info_get_filename (icon_info) : "",
                            /* Displayed when opening the browser for the first time. */
                            _("Welcome to Web"), _("Start browsing and your most-visited sites will appear here."));
    if (icon_info)
      g_object_unref (icon_info);
    goto out;
  }

  g_string_append (data_str,
                   "<div id=\"overview\">\n");

  for (l = urls; l; l = g_list_next (l)) {
    EphyHistoryURL *url = (EphyHistoryURL *)l->data;
    const char *snapshot;
    char *thumbnail_style = NULL;

    snapshot = ephy_snapshot_service_lookup_cached_snapshot_path (snapshot_service, url->url);
    if (snapshot)
      thumbnail_style = g_strdup_printf (" style=\"background: url(file://%s) no-repeat;\"", snapshot);
    else
      ephy_embed_shell_schedule_thumbnail_update (shell, url);

    g_string_append_printf (data_str,
                            "<a class=\"overview-item\" title=\"%s\" href=\"%s\">"
                            "  <div class=\"overview-close-button\" title=\"%s\">&#10006;</div>"
                            "  <span class=\"overview-thumbnail\"%s></span>"
                            "  <span class=\"overview-title\">%s</span>"
                            "</a>",
                            g_markup_escape_text (url->title, -1), url->url, _("Remove from overview"),
                            thumbnail_style ? thumbnail_style : "", url->title);
    g_free (thumbnail_style);
  }

  data_str = g_string_append (data_str,
                              "  </div>\n"
                              "</body></html>\n");

 out:
  data_length = data_str->len;
  ephy_about_handler_finish_request (request, g_string_free (data_str, FALSE), data_length);
  g_object_unref (request);
}

EphyHistoryQuery *
ephy_history_query_new_for_overview (void)
{
  EphyHistoryQuery *query;

  query = ephy_history_query_new ();
  query->sort_type = EPHY_HISTORY_SORT_MOST_VISITED;
  query->limit = EPHY_ABOUT_OVERVIEW_MAX_ITEMS;
  query->ignore_hidden = TRUE;
  query->ignore_local = TRUE;

  return query;
}

static gboolean
ephy_about_handler_handle_html_overview (EphyAboutHandler       *handler,
                                         WebKitURISchemeRequest *request)
{
  EphyHistoryService *history;
  EphyHistoryQuery *query;

  history = ephy_embed_shell_get_global_history_service (ephy_embed_shell_get_default ());
  query = ephy_history_query_new_for_overview ();
  ephy_history_service_query_urls (history, query, NULL,
                                   (EphyHistoryJobCallback)history_service_query_urls_cb,
                                   g_object_ref (request));
  ephy_history_query_free (query);

  return TRUE;
}

static gboolean
ephy_about_handler_handle_incognito (EphyAboutHandler       *handler,
                                     WebKitURISchemeRequest *request)
{
  char *data;

  if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) != EPHY_EMBED_SHELL_MODE_INCOGNITO)
    return FALSE;

  data = g_strdup_printf ("<html>\n"
                          "<div dir=\"%s\">\n"
                          "<head>\n"
                          "<title>%s</title>\n"
                          "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
                          "<link href=\""EPHY_PAGE_TEMPLATE_ABOUT_CSS "\" rel=\"stylesheet\" type=\"text/css\">\n"
                          "</head>\n"
                          "<body class=\"incognito-body\">\n"
                          "  <div id=\"mainblock\">\n"
                          "    <div style=\"background: transparent url(ephy-resource:///org/gnome/epiphany/incognito.png) no-repeat 10px center;\">\n" \
                          "      <h1>%s</h1>\n"
                          "      <p>%s</p>\n"
                          "      <p><strong>%s</strong> %s</p>\n"
                          "    </div>\n"
                          "  </div>\n"
                          "</body>\n"
                          "</div>\n"
                          "</html>\n",
                          gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL ? "rtl" : "ltr",
                          _("Private Browsing"),
                          _("Private Browsing"),
                          _("You are currently browsing incognito. Pages viewed in this "
                            "mode will not show up in your browsing history and all stored "
                            "information will be cleared when you close the window. Files you "
                            "download will be kept."),
                          _("Incognito mode hides your activity only from people using this "
                            "computer."),
                          _("It will not hide your activity from your employer if you are at "
                            "work. Your internet service provider, your government, other "
                            "governments, the websites that you visit, and advertisers on "
                            "these websites may still be tracking you."));

  ephy_about_handler_finish_request (request, data, -1);

  return TRUE;
}

static void
ephy_about_handler_handle_blank (EphyAboutHandler       *handler,
                                 WebKitURISchemeRequest *request)
{
  ephy_about_handler_finish_request (request, g_strdup ("<html></html>"), -1);
}

EphyAboutHandler *
ephy_about_handler_new (void)
{
  return EPHY_ABOUT_HANDLER (g_object_new (EPHY_TYPE_ABOUT_HANDLER, NULL));
}

void
ephy_about_handler_handle_request (EphyAboutHandler       *handler,
                                   WebKitURISchemeRequest *request)
{
  const char *path;
  gboolean handled = FALSE;

  path = webkit_uri_scheme_request_get_path (request);

  if (!g_strcmp0 (path, "memory"))
    handled = ephy_about_handler_handle_memory (handler, request);
  else if (!g_strcmp0 (path, "epiphany"))
    handled = ephy_about_handler_handle_epiphany (handler, request);
  else if (!g_strcmp0 (path, "applications") && !ephy_is_running_inside_flatpak ())
    handled = ephy_about_handler_handle_applications (handler, request);
  else if (!g_strcmp0 (path, "overview"))
    handled = ephy_about_handler_handle_html_overview (handler, request);
  else if (!g_strcmp0 (path, "incognito"))
    handled = ephy_about_handler_handle_incognito (handler, request);
  else if (path == NULL || path[0] == '\0' || !g_strcmp0 (path, "Web") || !g_strcmp0 (path, "web"))
    handled = ephy_about_handler_handle_about (handler, request);

  if (!handled)
    ephy_about_handler_handle_blank (handler, request);
}
