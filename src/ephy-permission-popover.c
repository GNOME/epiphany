/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2023 Jamie Murphy <hello@itsjamie.dev>
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

#include "ephy-embed-shell.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-permission-popover.h"

#include <glib/gi18n.h>

struct _EphyPermissionPopover {
  GtkPopover parent_instance;

  GtkLabel *permission_title;
  GtkLabel *permission_description;

  EphyPermissionType permission_type;
  WebKitPermissionRequest *permission_request;
  char *origin;
};

G_DEFINE_FINAL_TYPE (EphyPermissionPopover, ephy_permission_popover, GTK_TYPE_POPOVER)

enum {
  PROP_0,
  PROP_PERMISSION_TYPE,
  PROP_PERMISSION_REQUEST,
  PROP_ORIGIN,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

enum {
  ALLOW,
  DENY,
  LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

static void
on_permission_deny (EphyPermissionPopover *self)
{
  gtk_popover_popdown (GTK_POPOVER (self));
  g_signal_emit (self, signals[DENY], 0);
}

static void
on_permission_allow (EphyPermissionPopover *self)
{
  gtk_popover_popdown (GTK_POPOVER (self));
  g_signal_emit (self, signals[ALLOW], 0);
}

static void
ephy_permission_popover_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  EphyPermissionPopover *self = EPHY_PERMISSION_POPOVER (object);

  switch (prop_id) {
    case PROP_PERMISSION_TYPE:
      g_value_set_enum (value, self->permission_type);
      break;
    case PROP_PERMISSION_REQUEST:
      g_value_set_object (value, self->permission_request);
      break;
    case PROP_ORIGIN:
      g_value_set_string (value, self->origin);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_permission_popover_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  EphyPermissionPopover *self = EPHY_PERMISSION_POPOVER (object);

  switch (prop_id) {
    case PROP_PERMISSION_TYPE:
      self->permission_type = g_value_get_enum (value);
      break;
    case PROP_PERMISSION_REQUEST:
      self->permission_request = g_object_ref (g_value_get_object (value));
      break;
    case PROP_ORIGIN:
      self->origin = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_permission_popover_constructed (GObject *object)
{
  EphyPermissionPopover *self = EPHY_PERMISSION_POPOVER (object);
  g_autofree char *title = NULL;
  g_autofree char *message = NULL;

  ephy_permission_popover_get_text (self, &title, &message);

  gtk_label_set_label (self->permission_title, title);
  gtk_label_set_label (self->permission_description, message);
}

static void
ephy_permission_popover_dispose (GObject *object)
{
  EphyPermissionPopover *self = EPHY_PERMISSION_POPOVER (object);

  g_clear_object (&self->permission_request);

  G_OBJECT_CLASS (ephy_permission_popover_parent_class)->dispose (object);
}

static void
ephy_permission_popover_finalize (GObject *object)
{
  EphyPermissionPopover *self = EPHY_PERMISSION_POPOVER (object);

  g_free (self->origin);

  G_OBJECT_CLASS (ephy_permission_popover_parent_class)->finalize (object);
}

static void
ephy_permission_popover_class_init (EphyPermissionPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ephy_permission_popover_get_property;
  object_class->set_property = ephy_permission_popover_set_property;
  object_class->constructed = ephy_permission_popover_constructed;
  object_class->dispose = ephy_permission_popover_dispose;
  object_class->finalize = ephy_permission_popover_finalize;

  obj_properties[PROP_PERMISSION_TYPE] =
    g_param_spec_enum ("permission-type", "", "", EPHY_TYPE_PERMISSION_TYPE,
                       EPHY_PERMISSION_TYPE_SHOW_NOTIFICATIONS,
                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_PERMISSION_REQUEST] =
    g_param_spec_object ("permission-request", "", "", WEBKIT_TYPE_PERMISSION_REQUEST,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_ORIGIN] =
    g_param_spec_string ("origin", "", "", "",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  /**
   * EphyPermissionPopover::allow:
   * @popover: the object on which the signal is emitted
   *
   * Emitted when the user presses "Allow" on the popover prompt
   *
   */
  signals[ALLOW] = g_signal_new ("allow", G_OBJECT_CLASS_TYPE (klass),
                                 G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                                 0, NULL, NULL, NULL,
                                 G_TYPE_NONE,
                                 0);

  /**
   * EphyPermissionPopover::deny:
   * @popover: the object on which the signal is emitted
   *
   * Emitted when the user presses "Deny" on the popover prompt
   *
   */
  signals[DENY] = g_signal_new ("deny", G_OBJECT_CLASS_TYPE (klass),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL,
                                G_TYPE_NONE,
                                0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/permission-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyPermissionPopover, permission_title);
  gtk_widget_class_bind_template_child (widget_class, EphyPermissionPopover, permission_description);

  gtk_widget_class_bind_template_callback (widget_class, on_permission_deny);
  gtk_widget_class_bind_template_callback (widget_class, on_permission_allow);
}

static void
ephy_permission_popover_init (EphyPermissionPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

EphyPermissionPopover *
ephy_permission_popover_new (EphyPermissionType       permission_type,
                             WebKitPermissionRequest *permission_request,
                             const char              *origin)
{
  return g_object_new (EPHY_TYPE_PERMISSION_POPOVER,
                       "permission-type", permission_type,
                       "permission-request", permission_request,
                       "origin", origin,
                       NULL);
}

EphyPermissionType
ephy_permission_popover_get_permission_type (EphyPermissionPopover *self)
{
  g_assert (EPHY_IS_PERMISSION_POPOVER (self));

  return self->permission_type;
}

WebKitPermissionRequest *
ephy_permission_popover_get_permission_request (EphyPermissionPopover *self)
{
  g_assert (EPHY_IS_PERMISSION_POPOVER (self));

  return self->permission_request;
}

const char *
ephy_permission_popover_get_origin (EphyPermissionPopover *self)
{
  g_assert (EPHY_IS_PERMISSION_POPOVER (self));

  return g_strdup (self->origin);
}

void
ephy_permission_popover_get_text (EphyPermissionPopover  *self,
                                  char                  **title,
                                  char                  **message)
{
  const char *requesting_domain = NULL;
  const char *current_domain = NULL;
  g_autofree char *bold_origin = NULL;

  g_assert (EPHY_IS_PERMISSION_POPOVER (self));

  bold_origin = g_markup_printf_escaped ("<b>%s</b>", self->origin);
  switch (self->permission_type) {
    case EPHY_PERMISSION_TYPE_SHOW_NOTIFICATIONS:
      /* Translators: Notification policy for a specific site. */
      *title = g_strdup (_("Notification Request"));
      /* Translators: Notification policy for a specific site. */
      *message = g_strdup_printf (_("The page at “%s” would like to send you notifications"),
                                  bold_origin);
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_LOCATION:
      /* Translators: Geolocation policy for a specific site. */
      *title = g_strdup (_("Location Access Request"));
      /* Translators: Geolocation policy for a specific site. */
      *message = g_strdup_printf (_("The page at “%s” would like to know your location"),
                                  bold_origin);
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_MICROPHONE:
      /* Translators: Microphone policy for a specific site. */
      *title = g_strdup (_("Microphone Access Request"));
      /* Translators: Microphone policy for a specific site. */
      *message = g_strdup_printf (_("The page at “%s” would like to use your microphone"),
                                  bold_origin);
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_WEBCAM:
      /* Translators: Webcam policy for a specific site. */
      *title = g_strdup (_("Webcam Access Request"));
      /* Translators: Webcam policy for a specific site. */
      *message = g_strdup_printf (_("The page at “%s” would like to use your webcam"),
                                  bold_origin);
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_WEBCAM_AND_MICROPHONE:
      /* Translators: Webcam and microphone policy for a specific site. */
      *title = g_strdup (_("Webcam and Microphone Access Request"));
      /* Translators: Webcam and microphone policy for a specific site. */
      *message = g_strdup_printf (_("The page at “%s” would like to use your webcam and microphone"),
                                  bold_origin);
      break;
    case EPHY_PERMISSION_TYPE_WEBSITE_DATA_ACCESS:
      requesting_domain = webkit_website_data_access_permission_request_get_requesting_domain (WEBKIT_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST (self->permission_request));
      current_domain = webkit_website_data_access_permission_request_get_current_domain (WEBKIT_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST (self->permission_request));
      /* Translators: Storage access policy for a specific site. */
      *title = g_strdup (_("Website Data Access Request"));
      /* Translators: Storage access policy for a specific site. */
      *message = g_strdup_printf (_("The page at “%s” would like to access its own data (including cookies) while browsing “%s”. This will allow “%s” to track your activity on “%s”."),
                                  requesting_domain, current_domain, requesting_domain, current_domain);
      break;
    case EPHY_PERMISSION_TYPE_CLIPBOARD:
      /* Translators: Clipboard policy for a specific site. */
      *title = g_strdup (_("Clipboard Access Request"));
      /* Translators: Clipboard policy for a specific site. */
      *message = g_strdup_printf (_("The page at “%s” would like to access your clipboard"),
                                  bold_origin);
      break;
    default:
      g_assert_not_reached ();
  }
}
