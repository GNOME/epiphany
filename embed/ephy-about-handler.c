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

#include "ephy-file-helpers.h"
#include "ephy-smaps.h"
#include "ephy-web-app-utils.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#ifdef HAVE_WEBKIT2
#include <webkit2/webkit2.h>
#else
#include <webkit/webkit.h>
#endif

static gchar *css_style = NULL;

static void
read_css_style ()
{
  GError *error = NULL;
  const gchar *file;

  if (css_style)
    return;

  file = ephy_file ("about.css");
  if (file && !g_file_get_contents (file, &css_style, NULL, &error)) {
    g_debug ("%s", error->message);
    g_error_free (error);
  }
}

void
_ephy_about_handler_handle_plugins (GString *data_str, GList *plugin_list)
{
#ifdef HAVE_WEBKIT2
  GList *p;

  read_css_style ();

  g_string_append_printf (data_str, "<head><title>%s</title>"           \
                          "<style type=\"text/css\">%s</style></head><body>",
                          _("Installed plugins"),
                          css_style);

  g_string_append_printf (data_str, "<h1>%s</h1>", _("Installed plugins"));

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

  g_string_append (data_str, "</body>");
#endif
}

static void
ephy_about_handler_handle_plugins (GString *data_str)
{
#ifndef HAVE_WEBKIT2
  WebKitWebPluginDatabase* database = webkit_get_web_plugin_database ();
  GSList *plugin_list, *p;

  g_string_append_printf (data_str, "<head><title>%s</title>"           \
                          "<style type=\"text/css\">%s</style></head><body>",
                          _("Installed plugins"),
                          css_style);

  g_string_append_printf (data_str, "<h1>%s</h1>", _("Installed plugins"));
  plugin_list = webkit_web_plugin_database_get_plugins (database);

  for (p = plugin_list; p; p = p->next) {
    WebKitWebPlugin *plugin = WEBKIT_WEB_PLUGIN (p->data);
    GSList *m, *mime_types;

    g_string_append_printf (data_str, "<h2>%s</h2>%s<br>%s: <b>%s</b>"  \
                            "<table id=\"plugin-table\">"               \
                            "  <thead><tr><th>%s</th><th>%s</th><th>%s</th></tr></thead><tbody>",
                            webkit_web_plugin_get_name (plugin),
                            webkit_web_plugin_get_description (plugin),
                            _("Enabled"), webkit_web_plugin_get_enabled (plugin) ? _("Yes") : _("No"),
                            _("MIME type"), _("Description"), _("Suffixes"));

    mime_types = webkit_web_plugin_get_mimetypes (plugin);

    for (m = mime_types; m; m = m->next) {
      WebKitWebPluginMIMEType *mime_type = (WebKitWebPluginMIMEType*) m->data;
      guint i;

      g_string_append_printf (data_str, "<tr><td>%s</td><td>%s</td><td>",
                              mime_type->name, mime_type->description);

      for (i = 0; mime_type->extensions[i] != NULL; i++)
        g_string_append_printf (data_str, "%s%c", mime_type->extensions[i],
                                mime_type->extensions[i + 1] ? ',' : ' ');

      g_string_append (data_str, "</td></tr>");
    }

    g_string_append (data_str, "</tbody></table>");
  }

  webkit_web_plugin_database_plugins_list_free (plugin_list);
#endif
  g_string_append (data_str, "</body>");
}

static void
ephy_about_handler_handle_memory (GString *data_str)
{
  char *memory;
  static EphySMaps *smaps = NULL;
  if (!smaps)
    smaps = ephy_smaps_new ();

  memory = ephy_smaps_to_html (smaps);

  if (memory) {
    g_string_append_printf (data_str, "<head><title>%s</title>"         \
                            "<style type=\"text/css\">%s</style></head><body>",
                            _("Memory usage"),
                            css_style);

    g_string_append_printf (data_str, "<h1>%s</h1>", _("Memory usage"));
    g_string_append (data_str, memory);
    g_free (memory);
  }
}

static void
ephy_about_handler_handle_epiphany (GString *data_str)
{
  g_string_append_printf (data_str, "<head><title>Epiphany</title>"     \
                          "<style type=\"text/css\">%s</style></head>"  \
                          "<body style=\"background: #3369FF; color: white; font-style: italic;\">",
                          css_style);

  g_string_append (data_str, "<div id=\"ephytext\">"                    \
                   "Il semble que la perfection soit atteinte non quand il n'y a plus rien à" \
                   " ajouter, mais quand il n'y a plus rien à retrancher." \
                   "</div>"                                             \
                   "<div id=\"from\">"                                  \
                   "<!-- Terre des Hommes, III: L'Avion, p. 60 -->"     \
                   "Antoine de Saint-Exupéry"                           \
                   "</div></body>");
}

static void
ephy_about_handler_handle_applications (GString *data_str)
{
  GList *applications, *p;

  g_string_append_printf (data_str, "<head><title>%s</title>"           \
                          "<style type=\"text/css\">%s</style></head>"  \
                          "<body class=\"applications-body\"><h1>%s</h1>" \
                          "<p>%s</p>",
                          _("Applications"),
                          css_style,
                          _("Applications"),
                          _("List of installed web applications"));

  g_string_append (data_str, "<form><table>");

  applications = ephy_web_application_get_application_list ();
  for (p = applications; p; p = p->next) {
    char *img_data = NULL, *img_data_base64 = NULL;
    gsize data_length;
    EphyWebApplication *app = (EphyWebApplication*)p->data;

    if (g_file_get_contents (app->icon_url, &img_data, &data_length, NULL))
      img_data_base64 = g_base64_encode ((guchar*)img_data, data_length);
    g_string_append_printf (data_str, "<tbody><tr><td class=\"icon\"><img width=64 height=64 src=\"data:image/png;base64,%s\">" \
                            " </img></td><td class=\"data\"><div class=\"appname\">%s</div><div class=\"appurl\">%s</div></td><td class=\"input\"><input type=\"submit\" value=\"Delete\" id=\"%s\"></td><td class=\"date\">%s <br /> %s</td></tr>",
                            img_data_base64, app->name, app->url, app->name,
                            /* Note for translators: this refers to the installation date. */
                            _("Installed on:"), app->install_date);
    g_free (img_data_base64);
    g_free (img_data);
  }

  g_string_append (data_str, "</form></table></body>");

  ephy_web_application_free_application_list (applications);
}

GString *
ephy_about_handler_handle (const char *about)
{
  GString *data_str = g_string_new("<html>");

  read_css_style ();

  if (!g_strcmp0 (about, "plugins"))
    ephy_about_handler_handle_plugins (data_str);
  else if (!g_strcmp0 (about, "memory"))
    ephy_about_handler_handle_memory (data_str);
  else if (!g_strcmp0 (about, "epiphany"))
    ephy_about_handler_handle_epiphany (data_str);
  else if (!g_strcmp0 (about, "applications"))
    ephy_about_handler_handle_applications (data_str);

  g_string_append (data_str, "</html>");

  return data_str;
}
