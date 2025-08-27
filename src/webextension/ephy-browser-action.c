/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2022 Igalia S.L.
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

#include "ephy-browser-action.h"
#include "ephy-web-extension-manager.h"

struct _EphyBrowserAction {
  GObject parent_instance;
  EphyWebExtension *web_extension;
  char *badge_text;
  GdkRGBA *badge_color;
};

G_DEFINE_FINAL_TYPE (EphyBrowserAction, ephy_browser_action, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_WEB_EXTENSION,
  PROP_BADGE_TEXT,
  PROP_BADGE_COLOR,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

EphyBrowserAction *
ephy_browser_action_new (EphyWebExtension *web_extension)
{
  return g_object_new (EPHY_TYPE_BROWSER_ACTION,
                       "web-extension", web_extension,
                       NULL);
}

const char *
ephy_browser_action_get_title (EphyBrowserAction *self)
{
  const char *short_name = ephy_web_extension_get_short_name (self->web_extension);

  return short_name && *short_name ? short_name : ephy_web_extension_get_name (self->web_extension);
}

GdkPixbuf *
ephy_browser_action_get_pixbuf (EphyBrowserAction *self,
                                gint64             size)
{
  return ephy_web_extension_get_icon (self->web_extension, size);
}

EphyWebExtension *
ephy_browser_action_get_web_extension (EphyBrowserAction *self)
{
  return self->web_extension;
}

gboolean
ephy_browser_action_activate (EphyBrowserAction *self)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();

  /* If it has no popup clicking just emits this event and is handled already. */
  if (!ephy_web_extension_get_browser_popup (self->web_extension)) {
    ephy_web_extension_manager_emit_in_background_view (manager, self->web_extension, "browserAction.onClicked", "");
    return TRUE;
  }

  return FALSE;
}

static void
ephy_browser_action_finalize (GObject *object)
{
  EphyBrowserAction *self = (EphyBrowserAction *)object;

  g_clear_object (&self->badge_text);
  g_clear_object (&self->badge_color);
  g_clear_object (&self->web_extension);

  G_OBJECT_CLASS (ephy_browser_action_parent_class)->finalize (object);
}

static void
ephy_browser_action_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  EphyBrowserAction *self = EPHY_BROWSER_ACTION (object);

  switch (prop_id) {
    case PROP_WEB_EXTENSION:
      g_value_set_object (value, self->web_extension);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_browser_action_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  EphyBrowserAction *self = EPHY_BROWSER_ACTION (object);

  switch (prop_id) {
    case PROP_WEB_EXTENSION:
      g_set_object (&self->web_extension, g_value_dup_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_browser_action_class_init (EphyBrowserActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_browser_action_finalize;
  object_class->get_property = ephy_browser_action_get_property;
  object_class->set_property = ephy_browser_action_set_property;

  properties[PROP_WEB_EXTENSION] =
    g_param_spec_object ("web-extension",
                         NULL, NULL,
                         EPHY_TYPE_WEB_EXTENSION,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  properties[PROP_BADGE_TEXT] =
    g_param_spec_string ("badge-text",
                         "Badge Text",
                         "The badge text of the browser action",
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  properties[PROP_BADGE_COLOR] =
    g_param_spec_string ("badge-color",
                         "Badge Color",
                         "The badge color of the browser action",
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ephy_browser_action_init (EphyBrowserAction *self)
{
}

void
ephy_browser_action_set_badge_text (EphyBrowserAction *self,
                                    const char        *text)
{
  g_clear_pointer (&self->badge_text, g_free);

  if (text) {
    /* According to spec: Limit it to four chars max. */
    self->badge_text = g_strdup_printf ("%.4s", text);
  }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BADGE_TEXT]);
}

const char *
ephy_browser_action_get_badge_text (EphyBrowserAction *self)
{
  return self->badge_text;
}

void
ephy_browser_action_set_badge_background_color (EphyBrowserAction *self,
                                                GdkRGBA           *color)
{
  g_clear_pointer (&self->badge_color, gdk_rgba_free);
  self->badge_color = gdk_rgba_copy (color);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BADGE_COLOR]);
}

GdkRGBA *
ephy_browser_action_get_badge_background_color (EphyBrowserAction *self)
{
  return self->badge_color;
}
