/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*  Copyright © 2008 Xan Lopez <xan@gnome.org>
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

#include <glib.h>
#include <webkit/webkit.h>

#include "webkit-embed-prefs.h"
#include "eel-gconf-extensions.h"
#include "ephy-embed-prefs.h"

static GSList *embeds = NULL;
static WebKitWebSettings *settings = NULL;

static void
webkit_embed_prefs_apply (WebKitEmbed *embed, WebKitWebSettings *settings)
{
  webkit_web_view_set_settings (WEBKIT_WEB_VIEW (GTK_BIN (GTK_BIN (embed)->child)->child),
                                settings);
}

static void
notify_minimum_size_cb (GConfClient *client,
                        guint cnxn_id,
                        GConfEntry *entry,
                        gpointer data)
{
  GConfValue *gcvalue;
  gint size = 0;

  gcvalue = gconf_entry_get_value (entry);

  /* happens on initial notify if the key doesn't exist */
  if (gcvalue != NULL &&
      gcvalue->type == GCONF_VALUE_INT) {
      size = gconf_value_get_int (gcvalue);
      size = MAX (size, 0);
  }

  g_object_set (settings, "minimum-font-size", size, NULL);
}

static guint min_font_size_cnxn_id;

void
webkit_embed_prefs_init (void)
{
  eel_gconf_monitor_add ("/apps/epiphany/web");

  settings = webkit_web_settings_new ();

  min_font_size_cnxn_id = eel_gconf_notification_add (CONF_RENDERING_FONT_MIN_SIZE,
                                                      (GConfClientNotifyFunc) notify_minimum_size_cb,
                                                      NULL);

  eel_gconf_notify (CONF_RENDERING_FONT_MIN_SIZE);
}

void
webkit_embed_prefs_shutdown (void)
{
  eel_gconf_notification_remove (min_font_size_cnxn_id);

  g_object_unref (settings);

  g_slist_free (embeds);
}

void
webkit_embed_prefs_add_embed (WebKitEmbed *embed)
{
  embeds = g_slist_prepend (embeds, embed);

  webkit_embed_prefs_apply (embed, settings);
}

void
webkit_embed_prefs_remove_embed (WebKitEmbed *embed)
{
  embeds = g_slist_remove (embeds, embed);
}
