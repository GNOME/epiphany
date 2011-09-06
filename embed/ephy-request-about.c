/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2011 Igalia S.L.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "ephy-request-about.h"

#include "ephy-file-helpers.h"
#include "ephy-smaps.h"
#include "ephy-web-app-utils.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <libsoup/soup-uri.h>
#include <webkit/webkit.h>

G_DEFINE_TYPE (EphyRequestAbout, ephy_request_about, SOUP_TYPE_REQUEST)

struct _EphyRequestAboutPrivate {
  gssize content_length;
  gchar *css_style;
  EphySMaps *smaps;
};

static void
ephy_request_about_init (EphyRequestAbout *about)
{
  about->priv = G_TYPE_INSTANCE_GET_PRIVATE (about, EPHY_TYPE_REQUEST_ABOUT, EphyRequestAboutPrivate);
  about->priv->content_length = 0;
  about->priv->css_style = NULL;
  about->priv->smaps = ephy_smaps_new ();
}

static void
ephy_request_about_finalize (GObject *obj)
{
  EphyRequestAboutPrivate *priv = EPHY_REQUEST_ABOUT (obj)->priv;

  g_object_unref (priv->smaps);
  g_free (priv->css_style);

  G_OBJECT_CLASS (ephy_request_about_parent_class)->finalize (obj);
}

static gboolean
ephy_request_about_check_uri (SoupRequest  *request,
                              SoupURI      *uri,
                              GError      **error)
{
  return uri->host == NULL;
}

static void
read_css_style (EphyRequestAbout *about)
{
  GError *error = NULL;

  if (!g_file_get_contents (ephy_file ("about.css"), &about->priv->css_style, NULL, &error))
    g_debug (error->message);
}

static GInputStream *
ephy_request_about_send (SoupRequest          *request,
                         GCancellable         *cancellable,
                         GError              **error)
{
  EphyRequestAbout *about = EPHY_REQUEST_ABOUT (request);
  SoupURI *uri = soup_request_get_uri (request);
  GString *data_str = g_string_new("<html>");

  if (!about->priv->css_style)
    read_css_style (about);

  if (!g_strcmp0 (uri->path, "plugins")) {
    WebKitWebPluginDatabase* database = webkit_get_web_plugin_database ();
    GSList *plugin_list, *p;

    g_string_append_printf (data_str, "<head><title>%s</title>" \
                            "<style type=\"text/css\">%s</style></head><body>",
                            _("Installed plugins"),
                            about->priv->css_style);

    g_string_append_printf (data_str, "<h1>%s</h1>", _("Installed plugins"));
    plugin_list = webkit_web_plugin_database_get_plugins (database);

    for (p = plugin_list; p; p = p->next) {
      WebKitWebPlugin *plugin = WEBKIT_WEB_PLUGIN (p->data);
      GSList *m, *mime_types;

      g_string_append_printf (data_str, "<h2>%s</h2>%s<br>%s: <b>%s</b>"\
                              "<table id=\"plugin-table\">"             \
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
    g_string_append (data_str, "</body>");
  } else if (!g_strcmp0 (uri->path, "memory")) {
    char *memory = ephy_smaps_to_html (EPHY_REQUEST_ABOUT (request)->priv->smaps);

    if (memory) {
      g_string_append_printf (data_str, "<head><title>%s</title>"       \
                              "<style type=\"text/css\">%s</style></head><body>",
                              _("Memory usage"),
                              about->priv->css_style);
      
      g_string_append (data_str, memory);
      g_free (memory);
    }

  } else if (!g_strcmp0 (uri->path, "epiphany")) {
    g_string_append_printf (data_str, "<head><title>Epiphany</title>" \
                            "<style type=\"text/css\">%s</style></head>" \
                            "<body style=\"background: #3369FF; color: white; font-style: italic;\">",
                            about->priv->css_style);

    g_string_append (data_str, "<div id=\"ephytext\">" \
                     "Il semble que la perfection soit atteinte non quand il n'y a plus rien à" \
                     " ajouter, mais quand il n'y a plus rien à retrancher." \
                     "</div>" \
                     "<div id=\"from\">" \
                     "<!-- Terre des Hommes, III: L'Avion, p. 60 -->" \
                     "Antoine de Saint-Exupéry" \
                     "</div></body>");
  } else if (!g_strcmp0 (uri->path, "applications")) {
    GList *applications, *p;

    g_string_append_printf (data_str, "<head><title>%s</title>"   \
                            "<style type=\"text/css\">%s</style></head>" \
                            "<body class=\"applications-body\"><h1>%s</h1>" \
                            "<p>%s</p>",
                            _("Applications"),
                            about->priv->css_style,
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
                              img_data_base64, app->name, app->url, app->name, _("Installed on:"), app->install_date);
      g_free (img_data_base64);
      g_free (img_data);
    }

    g_string_append (data_str, "</form></table></body>");
    
    ephy_web_application_free_application_list (applications);
  }

  g_string_append (data_str, "</html>");
  about->priv->content_length = data_str->len;
  return g_memory_input_stream_new_from_data (g_string_free (data_str, false), about->priv->content_length, g_free);
}

static goffset
ephy_request_about_get_content_length (SoupRequest *request)
{
  return  EPHY_REQUEST_ABOUT (request)->priv->content_length;
}

static const char *
ephy_request_about_get_content_type (SoupRequest *request)
{
  return "text/html";
}

static const char *about_schemes[] = { EPHY_ABOUT_SCHEME, NULL };

static void
ephy_request_about_class_init (EphyRequestAboutClass *request_about_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (request_about_class);
  SoupRequestClass *request_class = SOUP_REQUEST_CLASS (request_about_class);

  gobject_class->finalize = ephy_request_about_finalize;

  request_class->schemes = about_schemes;
  request_class->check_uri = ephy_request_about_check_uri;
  request_class->send = ephy_request_about_send;
  request_class->get_content_length = ephy_request_about_get_content_length;
  request_class->get_content_type = ephy_request_about_get_content_type;

  g_type_class_add_private (request_about_class, sizeof (EphyRequestAboutPrivate));
}
