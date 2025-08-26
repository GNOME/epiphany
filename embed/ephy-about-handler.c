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
#include "ephy-output-encoding.h"
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

G_DEFINE_FINAL_TYPE (EphyAboutHandler, ephy_about_handler, G_TYPE_OBJECT)


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
                            "</head><body>"
                            "<div id='memory'>",
                            _("Memory usage"));

    g_string_append_printf (data_str, "<h1>%s</h1>", _("Memory usage"));
    g_string_append (data_str, memory);
    g_free (memory);

    g_string_append (data_str, "</div>");
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
  g_autofree char *path = NULL;
  GtkIconTheme *icon_theme;
  g_autoptr (GtkIconPaintable) paintable = NULL;

  version = g_strdup_printf (_("Version %s"), VERSION);

  icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
  paintable = gtk_icon_theme_lookup_icon (icon_theme,
                                          APPLICATION_ID,
                                          NULL,
                                          256,
                                          1,
                                          GTK_TEXT_DIR_LTR,
                                          GTK_ICON_LOOKUP_FORCE_REGULAR);

  if (paintable) {
    g_autoptr (GFile) file = gtk_icon_paintable_get_file (paintable);

    path = g_file_get_path (file);
  }

  data = g_strdup_printf ("<html><head><title>%s</title>"
                          "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
                          "<link href=\""EPHY_PAGE_TEMPLATE_ABOUT_CSS "\" rel=\"stylesheet\" type=\"text/css\">"
                          "</head><body>"
                          "<div id=\"about-app\">"
                          "<div class=\"dialog\">"
                          "<img id=\"about-icon\" src=\"file://%s\"/>"
                          "<h1 id=\"about-title\">%s</h1>"
                          "<h2 id=\"about-subtitle\">%s</h2>"
                          "<p id=\"about-tagline\">%s</p>"
                          "<table class=\"properties\">"
                          "<tr><td class=\"prop-label\">%s</td><td class=\"prop-value\">%d.%d.%d</td></tr>"
                          "</table>"
                          "</div></div></body></html>",
                          _("About Web"),
                          path ? path : "",
#if !TECH_PREVIEW
                          _("Web"),
#else
                          _("Epiphany Technology Preview"),
#endif
                          version,
                          _("A simple, clean, beautiful view of the web"),
                          "WebKitGTK", webkit_get_major_version (), webkit_get_minor_version (), webkit_get_micro_version ());
  g_free (version);

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
                          "« Il semble que la perfection soit atteinte non quand il n'y a plus rien à"
                          " ajouter, mais quand il n'y a plus rien à retrancher. »"
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
  WebKitWebView *view;
  GString *data_str;
  gsize data_length;
  GList *applications, *p;

  view = webkit_uri_scheme_request_get_web_view (request);
  ephy_web_view_register_message_handler (EPHY_WEB_VIEW (view), EPHY_WEB_VIEW_ABOUT_APPS_MESSAGE_HANDLER, EPHY_WEB_VIEW_REGISTER_MESSAGE_HANDLER_FOR_CURRENT_PAGE);

  data_str = g_string_new (NULL);
  applications = g_task_propagate_pointer (G_TASK (result), NULL);

  if (g_list_length (applications) > 0) {
    g_string_append_printf (data_str, "<html><head><title>%s</title>"
                            "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
                            "<link href=\""EPHY_PAGE_TEMPLATE_ABOUT_CSS "\" rel=\"stylesheet\" type=\"text/css\">"
                            "<script>"
                            "  function launchWebApp(appID, appName) {"
                            "    window.webkit.messageHandlers.aboutApps.postMessage({action: 'launch', app: appID, name: appName, page: %" G_GUINT64_FORMAT "});"
                            "  }"
                            "  function deleteWebApp(appID, appName) {"
                            "    window.webkit.messageHandlers.aboutApps.postMessage({action: 'remove', app: appID, name: appName, page: %" G_GUINT64_FORMAT "});"
                            "  }"
                            "</script>"
                            "</head><div id=\"applications\"><body class=\"applications-body\"><h1>%s</h1>"
                            "<p>%s</p>",
                            _("Apps"),
                            webkit_web_view_get_page_id (view),
                            webkit_web_view_get_page_id (view),
                            _("Apps"),
                            _("List of installed web apps"));

    g_string_append (data_str, "<table>");

    for (p = applications; p; p = p->next) {
      EphyWebApplication *app = (EphyWebApplication *)p->data;
      const char *icon_path = NULL;
      g_autofree char *encoded_icon_path = NULL;
      g_autofree char *encoded_name = NULL;
      g_autofree char *encoded_url = NULL;
      g_autoptr (GDate) date = NULL;
      char install_date[128];

      if (ephy_web_application_is_system (app))
        continue;

      date = g_date_new ();
      g_date_set_time_t (date, (time_t)app->install_date_uint64);
      g_date_strftime (install_date, 127, "%x", date);

      /* In the sandbox we don't have access to the host side icon file */
      if (ephy_is_running_inside_sandbox ())
        icon_path = app->tmp_icon_path;
      else
        icon_path = app->icon_path;

      if (!icon_path) {
        g_warning ("Failed to get icon path for app %s", app->id);
        continue;
      }

      /* Most of these fields are at least semi-trusted. The app ID was chosen
       * by ephy so it's safe. The icon URL could be changed by the user to
       * something else after web app creation, though, so better not fully
       * trust it. Then the app name and the main URL could contain anything
       * at all, so those need to be encoded for sure. Install date should be
       * fine because it's constructed by Epiphany.
       */
      encoded_icon_path = ephy_encode_for_html_attribute (icon_path);
      encoded_name = ephy_encode_for_html_entity (app->name);
      encoded_url = ephy_encode_for_html_entity (app->url);
      g_string_append_printf (data_str,
                              "<tbody><tr id =\"%s\">"
                              "<td class=\"icon\"><img width=64 height=64 src=\"file://%s\"></img></td>"
                              "<td class=\"data\"><div class=\"appname\">%s</div><div class=\"appurl\">%s</div></td>"
                              "<td class=\"input\"><input type=\"button\" value=\"%s\" "
                              "onclick=\"const appRow = this.closest('tr'); launchWebApp(appRow.id, appRow.querySelector('.appname').innerText);\" "
                              "class=\"suggested-action\"></td>  "
                              "<td class=\"input\"><input type=\"button\" value=\"%s\" "
                              "onclick=\"const appRow = this.closest('tr'); deleteWebApp(appRow.id, appRow.querySelector('.appname').innerText);\" "
                              "class=\"destructive-action\"></td>"
                              "<td class=\"date\">%s <br /> %s</td></tr></tbody>",
                              app->id, encoded_icon_path, encoded_name, encoded_url, _("Launch"), _("Delete"),
                              /* Note for translators: this refers to the installation date. */
                              _("Installed on:"), install_date);
    }

    g_string_append (data_str, "</table></div></body></html>");
  } else {
    GtkIconTheme *icon_theme;
    g_autoptr (GtkIconPaintable) paintable = NULL;
    g_autofree char *path = NULL;

    g_string_append_printf (data_str, "<html><head><title>%s</title>"
                            "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
                            "<link href=\""EPHY_PAGE_TEMPLATE_ABOUT_CSS "\" rel=\"stylesheet\" type=\"text/css\">"
                            "</head><body class=\"applications-body\">",
                            _("Apps"));

    icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
    paintable = gtk_icon_theme_lookup_icon (icon_theme,
                                            "application-x-addon-symbolic",
                                            NULL,
                                            128,
                                            1,
                                            GTK_TEXT_DIR_LTR,
                                            0);

    if (paintable) {
      g_autoptr (GFile) file = gtk_icon_paintable_get_file (paintable);

      path = g_file_get_path (file);
    }

    g_string_append_printf (data_str,
                            "  <div id=\"overview\" class=\"overview-empty\">\n"
                            "    <img src=\"file://%s\"/>\n"
                            "    <div><h1>%s</h1></div>\n"
                            "    <div><p>%s</p></div>\n"
                            "  </div>\n"
                            "</body></html>\n",
                            path ? path : "",
                            /* Displayed when opening applications without any installed web apps. */
                            _("Apps"), _("You can add your favorite website by clicking <b>Install as Web App…</b> within the page menu."));
  }

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
  guint list_length;

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
                          _(NEW_TAB_PAGE_TITLE));
  g_free (lang);

  list_length = g_list_length (urls);

  if (list_length == 0 || !success) {
    GtkIconTheme *icon_theme;
    g_autoptr (GtkIconPaintable) paintable = NULL;
    g_autofree char *path = NULL;
    g_autofree char *icon = g_strconcat (APPLICATION_ID, "-symbolic", NULL);

    icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
    paintable = gtk_icon_theme_lookup_icon (icon_theme,
                                            icon,
                                            NULL,
                                            128,
                                            1,
                                            GTK_TEXT_DIR_LTR,
                                            0);

    if (paintable) {
      g_autoptr (GFile) file = gtk_icon_paintable_get_file (paintable);

      path = g_file_get_path (file);
    }

    g_string_append_printf (data_str,
                            "  <div id=\"overview\" class=\"overview-empty\">\n"
                            "    <img src=\"file://%s\"/>\n"
                            "    <div><h1>%s</h1></div>\n"
                            "    <div><p>%s</p></div>\n"
                            "  </div>\n"
                            "</body></html>\n",
                            path ? path : "",
                            /* Displayed when opening the browser for the first time. */
                            _("Welcome to Web"), _("Start browsing and your most-visited sites will appear here."));
    goto out;
  }

  g_string_append (data_str,
                   "<div id=\"overview\">\n");

  g_string_append (data_str,
                   "<div id=\"most-visited-grid\">\n");

  for (l = urls; l; l = g_list_next (l)) {
    EphyHistoryURL *url = (EphyHistoryURL *)l->data;
    const char *snapshot;
    g_autofree char *thumbnail_style = NULL;
    g_autofree char *entity_encoded_title = NULL;
    g_autofree char *attribute_encoded_title = NULL;
    g_autofree char *encoded_url = NULL;

    snapshot = ephy_snapshot_service_lookup_cached_snapshot_path (snapshot_service, url->url);
    if (snapshot)
      thumbnail_style = g_strdup_printf (" style=\"background: url(file://%s) no-repeat; background-size: 100%%;\"", snapshot);
    else
      ephy_embed_shell_schedule_thumbnail_update (shell, url);

    /* Title and URL are controlled by web content and could be malicious. */
    entity_encoded_title = ephy_encode_for_html_entity (url->title);
    attribute_encoded_title = ephy_encode_for_html_attribute (url->title);
    encoded_url = ephy_encode_for_html_attribute (url->url);
    g_string_append_printf (data_str,
                            "<a class=\"overview-item\" title=\"%s\" href=\"%s\">"
                            "  <div class=\"overview-close-button\" title=\"%s\"></div>"
                            "  <span class=\"overview-thumbnail\"%s></span>"
                            "  <span class=\"overview-title\">%s</span>"
                            "</a>",
                            attribute_encoded_title, encoded_url, _("Remove from overview"),
                            thumbnail_style ? thumbnail_style : "",
                            entity_encoded_title);
  }

  data_str = g_string_append (data_str,
                              "  </div>\n"
                              "  </div>\n"
                              "</body></html>\n");

out:
  data_length = data_str->len;
  ephy_about_handler_finish_request (request, g_string_free (data_str, FALSE), data_length);
  g_object_unref (request);
}

static gboolean
ephy_about_handler_handle_newtab (EphyAboutHandler       *handler,
                                  WebKitURISchemeRequest *request)
{
  char *data;

  data = g_strdup_printf ("<html><head><title>%s</title>"
                          "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
                          "</head><body style=\"color-scheme: light dark;\">"
                          "</body></html>",
                          _(NEW_TAB_PAGE_TITLE));

  ephy_about_handler_finish_request (request, data, -1);

  return TRUE;
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
                          "  <img class=\"incognito-body-image\" src=\"ephy-resource:///org/gnome/epiphany/page-icons/private-mode.svg\">\n" \
                          "  <br/>\n"
                          "  <h1>%s</h1>\n"
                          "  <p>%s</p>\n"
                          "  <p><strong>%s</strong> %s</p>\n"
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
  else if (!g_strcmp0 (path, "applications"))
    handled = ephy_about_handler_handle_applications (handler, request);
  else if (!g_strcmp0 (path, "newtab"))
    handled = ephy_about_handler_handle_newtab (handler, request);
  else if (!g_strcmp0 (path, "overview"))
    handled = ephy_about_handler_handle_html_overview (handler, request);
  else if (!g_strcmp0 (path, "incognito"))
    handled = ephy_about_handler_handle_incognito (handler, request);
  else if (!path || path[0] == '\0' || !g_strcmp0 (path, "Web") || !g_strcmp0 (path, "web"))
    handled = ephy_about_handler_handle_about (handler, request);

  if (!handled)
    ephy_about_handler_handle_blank (handler, request);
}
