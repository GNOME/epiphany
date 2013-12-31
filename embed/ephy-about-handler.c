/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ephy-about-handler.h"

#include "ephy-embed-shell.h"
#include "ephy-file-helpers.h"
#include "ephy-smaps.h"
#include "ephy-web-app-utils.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

struct _EphyAboutHandlerPrivate {
  char *style_sheet;
  EphySMaps *smaps;
};

G_DEFINE_TYPE (EphyAboutHandler, ephy_about_handler, G_TYPE_OBJECT)

static void
ephy_about_handler_finalize (GObject *object)
{
  EphyAboutHandler *handler = EPHY_ABOUT_HANDLER (object);

  g_free (handler->priv->style_sheet);
  g_clear_object (&handler->priv->smaps);

  G_OBJECT_CLASS (ephy_about_handler_parent_class)->finalize (object);
}

static void
ephy_about_handler_init (EphyAboutHandler *handler)
{
  handler->priv = G_TYPE_INSTANCE_GET_PRIVATE (handler, EPHY_TYPE_ABOUT_HANDLER, EphyAboutHandlerPrivate);
}

static void
ephy_about_handler_class_init (EphyAboutHandlerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_about_handler_finalize;
  g_type_class_add_private (object_class, sizeof (EphyAboutHandlerPrivate));
}

static const char *
ephy_about_handler_get_style_sheet (EphyAboutHandler *handler)
{
  if (!handler->priv->style_sheet) {
    const gchar *file;
    GError *error = NULL;

    file = ephy_file ("about.css");
    if (file && !g_file_get_contents (file, &handler->priv->style_sheet, NULL, &error)) {
      g_debug ("%s", error->message);
      g_error_free (error);
    }
  }

  return handler->priv->style_sheet;
}

static EphySMaps *
ephy_about_handler_get_smaps (EphyAboutHandler *handler)
{
  if (!handler->priv->smaps)
    handler->priv->smaps = ephy_smaps_new ();

  return handler->priv->smaps;
}

static void
ephy_about_handler_finish_request (WebKitURISchemeRequest *request,
                                   gchar *data,
                                   gsize data_length)
{
  GInputStream *stream;

  data_length = data_length != -1 ? data_length : strlen (data);
  stream = g_memory_input_stream_new_from_data (data, data_length, g_free);
  webkit_uri_scheme_request_finish (request, stream, data_length, "text/html");
  g_object_unref (stream);
}

typedef struct {
  EphyAboutHandler *handler;
  WebKitURISchemeRequest *request;
} EphyAboutRequest;

static EphyAboutRequest *
ephy_about_request_new (EphyAboutHandler *handler,
                        WebKitURISchemeRequest *request)
{
  EphyAboutRequest *about_request;

  about_request = g_slice_new (EphyAboutRequest);
  about_request->handler = g_object_ref (handler);
  about_request->request = g_object_ref (request);

  return about_request;
}

static void
ephy_about_request_free (EphyAboutRequest *about_request)
{
  g_object_unref (about_request->handler);
  g_object_unref (about_request->request);

  g_slice_free (EphyAboutRequest, about_request);
}

static void
get_plugins_cb (WebKitWebContext *web_context,
                GAsyncResult *result,
                EphyAboutRequest *about_request)
{
  GString *data_str;
  gsize data_length;
  GList *plugin_list, *p;

  data_str = g_string_new ("<html>");
  g_string_append_printf (data_str, "<head><title>%s</title>"           \
                          "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />" \
                          "<style type=\"text/css\">%s</style></head><body>",
                          _("Installed plugins"),
                          ephy_about_handler_get_style_sheet (about_request->handler));
  g_string_append_printf (data_str, "<h1>%s</h1>", _("Installed plugins"));

  plugin_list = webkit_web_context_get_plugins_finish (web_context, result, NULL);
  for (p = plugin_list; p; p = p->next) {
    WebKitPlugin *plugin = WEBKIT_PLUGIN (p->data);
    GList *m, *mime_types;

    /* TODO: Enable/disable plugins in WebKit2 */
    g_string_append_printf (data_str, "<h2>%s</h2>%s<br>%s: <b>%s</b>"  \
                            "<table id=\"plugin-table\">"               \
                            "  <thead><tr><th>%s</th><th>%s</th><th>%s</th></tr></thead><tbody>",
                            webkit_plugin_get_name (plugin),
                            webkit_plugin_get_description (plugin),
                            _("Enabled"), /*webkit_plugin_get_enabled (plugin)*/ TRUE ? _("Yes") : _("No"),
                            _("MIME type"), _("Description"), _("Suffixes"));

    mime_types = webkit_plugin_get_mime_info_list (plugin);

    for (m = mime_types; m; m = m->next) {
      WebKitMimeInfo *mime_info = (WebKitMimeInfo *) m->data;
      const gchar * const *extensions;
      guint i;

      g_string_append_printf (data_str, "<tr><td>%s</td><td>%s</td><td>",
                              webkit_mime_info_get_mime_type (mime_info),
                              webkit_mime_info_get_description (mime_info));

      extensions = webkit_mime_info_get_extensions (mime_info);
      for (i = 0; extensions && extensions[i] != NULL; i++)
        g_string_append_printf (data_str, "%s%c", extensions[i],
                                extensions[i + 1] ? ',' : ' ');

      g_string_append (data_str, "</td></tr>");
    }

    g_string_append (data_str, "</tbody></table>");
  }
  g_string_append (data_str, "</body></html>");

  g_list_free_full (plugin_list, g_object_unref);

  data_length = data_str->len;
  ephy_about_handler_finish_request (about_request->request, g_string_free (data_str, FALSE), data_length);
  ephy_about_request_free (about_request);
}

static gboolean
ephy_about_handler_handle_plugins (EphyAboutHandler *handler,
                                   WebKitURISchemeRequest *request)
{
  webkit_web_context_get_plugins (webkit_web_context_get_default (),
                                  NULL,
                                  (GAsyncReadyCallback)get_plugins_cb,
                                  ephy_about_request_new (handler, request));

  return TRUE;
}

static void
handle_memory_finished_cb (EphyAboutHandler *handler,
                           GAsyncResult *result,
                           WebKitURISchemeRequest *request)
{
  GString *data_str;
  gsize data_length;
  char *memory;

  data_str = g_string_new ("<html>");

  memory = g_task_propagate_pointer (G_TASK (result), NULL);
  if (memory) {
    g_string_append_printf (data_str, "<head><title>%s</title>"         \
                            "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />" \
                            "<style type=\"text/css\">%s</style></head><body>",
                            _("Memory usage"),
                            ephy_about_handler_get_style_sheet (handler));

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
handle_memory_sync (GTask *task,
                    gpointer source_object,
                    gpointer task_data,
                    GCancellable *cancellable)
{
  EphyAboutHandler *handler = EPHY_ABOUT_HANDLER (source_object);

  g_task_return_pointer (task,
                         ephy_smaps_to_html (ephy_about_handler_get_smaps (handler)),
                         g_free);
}

static gboolean
ephy_about_handler_handle_memory (EphyAboutHandler *handler,
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
ephy_about_handler_handle_about (EphyAboutHandler *handler,
                                 WebKitURISchemeRequest *request)
{
  char *data;
  char *version;
  char *image_data = NULL;
  const char *filename;
  GtkIconInfo *icon_info;

  version = g_strdup_printf (_("Version %s"), VERSION);

  icon_info = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default (),
                                          "web-browser",
                                          256,
                                          GTK_ICON_LOOKUP_GENERIC_FALLBACK);
  if (icon_info != NULL) {
    filename = gtk_icon_info_get_filename (icon_info);
    image_data = ephy_file_create_data_uri_for_filename (filename, NULL);
  }

  data = g_strdup_printf ("<html><head><title>%s</title>"
                          "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
                          "<style type=\"text/css\">%s</style></head>"  \
                          "<body>"
                          "<div class=\"dialog\">"
                          "<img src=\"%s\"/>"
                          "<h1 id=\"about-title\">%s</h1>"
                          "<h2 id=\"about-subtitle\">%s</h2>"
                          "<p id=\"about-tagline\">“%s”</p>"
                          "<table class=\"properties\">"
                          "<tr><td class=\"prop-label\">%s</td><td class=\"prop-value\">%d.%d.%d</td></tr>"
                          "</table>"
                          "</div></body></html>",
                          _("About Web"),
                          ephy_about_handler_get_style_sheet (handler),
                          image_data ? image_data : "",
                          _("Web"),
                          version,
                          _("A simple, clean, beautiful view of the web"),
                          "WebKit", webkit_get_major_version (), webkit_get_minor_version (), webkit_get_micro_version ());
  g_free (version);
  g_free (image_data);

  ephy_about_handler_finish_request (request, data, -1);

  return TRUE;
}

static gboolean
ephy_about_handler_handle_epiphany (EphyAboutHandler *handler,
                                    WebKitURISchemeRequest *request)
{
  char *data;

  data = g_strdup_printf ("<html><head><title>%s</title>"
                          "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
                          "<style type=\"text/css\">%s</style></head>"
                          "<body class=\"epiphany-body\">"
                          "<div id=\"ephytext\">"
                          "“Il semble que la perfection soit atteinte non quand il n'y a plus rien à"
                          " ajouter, mais quand il n'y a plus rien à retrancher.”"
                          "</div>"
                          "<div id=\"from\">"
                          "<!-- Terre des Hommes, III: L'Avion, p. 60 -->"
                          "Antoine de Saint-Exupéry"
                          "</div></body></html>",
                          _("Web"),
                          ephy_about_handler_get_style_sheet (handler));

  ephy_about_handler_finish_request (request, data, -1);

  return TRUE;
}

static void
handle_applications_finished_cb (EphyAboutHandler *handler,
                                 GAsyncResult *result,
                                 WebKitURISchemeRequest *request)
{
  GString *data_str;
  gsize data_length;
  GList *applications, *p;

  data_str = g_string_new (NULL);
  g_string_append_printf (data_str, "<html><head><title>%s</title>" \
                          "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />" \
                          "<style type=\"text/css\">%s</style></head>"  \
                          "<body class=\"applications-body\"><h1>%s</h1>" \
                          "<p>%s</p>",
                          _("Applications"),
                          ephy_about_handler_get_style_sheet (handler),
                          _("Applications"),
                          _("List of installed web applications"));

  g_string_append (data_str, "<table>");

  applications = g_task_propagate_pointer (G_TASK (result), NULL);
  for (p = applications; p; p = p->next) {
    char *img_data = NULL, *img_data_base64 = NULL;
    gsize data_length;
    EphyWebApplication *app = (EphyWebApplication*)p->data;

    if (g_file_get_contents (app->icon_url, &img_data, &data_length, NULL))
      img_data_base64 = g_base64_encode ((guchar*)img_data, data_length);
    g_string_append_printf (data_str,
                            "<form>" \
                            "<tbody><tr>" \
                            "<td class=\"icon\"><img width=64 height=64 src=\"data:image/png;base64,%s\"></img></td>" \
                            "<td class=\"data\"><div class=\"appname\">%s</div><div class=\"appurl\">%s</div></td>" \
                            "<td class=\"input\"><input type=\"hidden\" name=\"app_id\" value=\"%s\"><input type=\"submit\" value=\"Delete\" id=\"%s\">" \
                            "</td><td class=\"date\">%s <br /> %s</td></tr></tbody></form>",
                            img_data_base64, app->name, app->url, app->name, app->name,
                            /* Note for translators: this refers to the installation date. */
                            _("Installed on:"), app->install_date);
    g_free (img_data_base64);
    g_free (img_data);
  }

  g_string_append (data_str, "</table></body></html>");

  ephy_web_application_free_application_list (applications);

  data_length = data_str->len;
  ephy_about_handler_finish_request (request, g_string_free (data_str, FALSE), data_length);
  g_object_unref (request);
}

static void
handle_applications_sync (GTask *task,
                          gpointer source_object,
                          gpointer task_data,
                          GCancellable *cancellable)
{
  g_task_return_pointer (task,
                         ephy_web_application_get_application_list (),
                         (GDestroyNotify)ephy_web_application_free_application_list);
}

static gboolean
ephy_about_handler_handle_applications (EphyAboutHandler *handler,
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

static gboolean
ephy_about_handler_handle_incognito (EphyAboutHandler *handler,
                                     WebKitURISchemeRequest *request)
{
  const char *filename;
  char *img_data = NULL, *img_data_base64 = NULL;
  char *data;
  gsize data_length;

  if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) != EPHY_EMBED_SHELL_MODE_INCOGNITO)
    return FALSE;

  filename = ephy_file ("incognito.png");
  if (filename) {
    g_file_get_contents (filename, &img_data, &data_length, NULL);
    img_data_base64 = g_base64_encode ((guchar*)img_data, data_length);
  }

  data = g_strdup_printf ("<html>\n"                                   \
                          "<head>\n"                                   \
                          "<title>%s</title>\n"                        \
                          "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />" \
                          "<style type=\"text/css\">%s</style>\n"      \
                          "</head>\n"                                  \
                          "<body class=\"incognito-body\">\n"          \
                          "  <div id=\"mainblock\">\n"                 \
                          "    <div style=\"background: transparent url(data:image/png;base64,%s) no-repeat 10px center;\">\n" \
                          "      <h1>%s</h1>\n"                        \
                          "      <p>%s</p>\n"                          \
                          "    </div>\n"                               \
                          "  </div>\n"                                 \
                          "</body>\n"                                  \
                          "</html>\n",
                          _("Private Browsing"),
                          ephy_about_handler_get_style_sheet (handler),
                          img_data_base64 ? img_data_base64 : "",
                          _("Private Browsing"),
                          _("You are currently browsing <em>incognito</em>. Pages viewed in this "
                            "mode will not show up in your browsing history and all stored "
                            "information will be cleared when you close the window."));

  g_free (img_data_base64);
  g_free (img_data);

  ephy_about_handler_finish_request (request, data, -1);

  return TRUE;
}

static void
ephy_about_handler_handle_blank (EphyAboutHandler *handler,
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
ephy_about_handler_handle_request (EphyAboutHandler *handler,
                                   WebKitURISchemeRequest *request)
{
  const char *path;
  gboolean    handled = FALSE;

  path = webkit_uri_scheme_request_get_path (request);

  if (!g_strcmp0 (path, "plugins"))
    handled = ephy_about_handler_handle_plugins (handler, request);
  else if (!g_strcmp0 (path, "memory"))
    handled = ephy_about_handler_handle_memory (handler, request);
  else if (!g_strcmp0 (path, "epiphany"))
    handled =  ephy_about_handler_handle_epiphany (handler, request);
  else if (!g_strcmp0 (path, "applications"))
    handled = ephy_about_handler_handle_applications (handler, request);
  else if (!g_strcmp0 (path, "incognito"))
    handled = ephy_about_handler_handle_incognito (handler, request);
  else if (path == NULL || path[0] == '\0' || !g_strcmp0 (path, "Web") || !g_strcmp0 (path, "web"))
    handled = ephy_about_handler_handle_about (handler, request);

  if (!handled)
    ephy_about_handler_handle_blank (handler, request);
}
