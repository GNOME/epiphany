/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * ephy-request-about.c: about: URI request object
 *
 * Copyright (C) 2011, Igalia S.L.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <libsoup/soup-uri.h>
#include <webkit/webkit.h>

#include "ephy-file-helpers.h"
#include "ephy-request-about.h"

G_DEFINE_TYPE (EphyRequestAbout, ephy_request_about, SOUP_TYPE_REQUEST)

struct _EphyRequestAboutPrivate {
  gssize content_length;
  gchar *css_style;
};

static void
ephy_request_about_init (EphyRequestAbout *about)
{
  about->priv = G_TYPE_INSTANCE_GET_PRIVATE (about, EPHY_TYPE_REQUEST_ABOUT, EphyRequestAboutPrivate);
  about->priv->content_length = 0;
  about->priv->css_style = NULL;
}

static void
ephy_request_about_finalize (GObject *obj)
{
  g_free (EPHY_REQUEST_ABOUT (obj)->priv->css_style);

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
