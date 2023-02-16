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

#include "ephy-password-popover.h"

#include "ephy-window.h"

struct _EphyPasswordPopover {
  GtkPopover parent_instance;

  GtkWidget *username_entry;
  GtkWidget *password_entry;

  EphyPasswordRequestData *request_data;
};

G_DEFINE_FINAL_TYPE (EphyPasswordPopover, ephy_password_popover, GTK_TYPE_POPOVER)

enum {
  PROP_0,
  PROP_REQUEST_DATA,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

enum {
  RESPONSE,
  LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

static void
on_entry_changed (EphyPasswordPopover *self,
                  GtkEditable         *entry)
{
  const char *text = gtk_editable_get_text (entry);

  if (entry == GTK_EDITABLE (self->username_entry))
    self->request_data->username = g_strdup (text);

  if (entry == GTK_EDITABLE (self->password_entry))
    self->request_data->password = g_strdup (text);
}

static void
on_password_never (EphyPasswordPopover *self,
                   GtkButton           *button)
{
  EphyEmbedShell *shell = EPHY_EMBED_SHELL (ephy_embed_shell_get_default ());
  EphyPermissionsManager *permissions_manager = ephy_embed_shell_get_permissions_manager (shell);

  ephy_permissions_manager_set_permission (permissions_manager,
                                           EPHY_PERMISSION_TYPE_SAVE_PASSWORD,
                                           self->request_data->origin,
                                           EPHY_PERMISSION_DENY);

  gtk_popover_popdown (GTK_POPOVER (self));
  g_signal_emit (self, signals[RESPONSE], 0);
}

static void
on_password_save (EphyPasswordPopover *self,
                  GtkButton           *button)
{
  EphyEmbedShell *shell = EPHY_EMBED_SHELL (ephy_embed_shell_get_default ());
  EphyPasswordManager *password_manager = ephy_embed_shell_get_password_manager (shell);

  ephy_password_manager_save (password_manager, self->request_data->origin,
                              self->request_data->target_origin, self->request_data->username,
                              self->request_data->password, self->request_data->usernameField,
                              self->request_data->passwordField, self->request_data->isNew);

  gtk_popover_popdown (GTK_POPOVER (self));
  g_signal_emit (self, signals[RESPONSE], 0);
}

static void
ephy_password_popover_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  EphyPasswordPopover *self = EPHY_PASSWORD_POPOVER (object);

  switch (prop_id) {
    case PROP_REQUEST_DATA:
      g_value_set_pointer (value, self->request_data);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_password_popover_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  EphyPasswordPopover *self = EPHY_PASSWORD_POPOVER (object);

  switch (prop_id) {
    case PROP_REQUEST_DATA:
      self->request_data = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_password_popover_constructed (GObject *object)
{
  EphyPasswordPopover *self = EPHY_PASSWORD_POPOVER (object);

  G_OBJECT_CLASS (ephy_password_popover_parent_class)->constructed (object);

  if (self->request_data->username) {
    gtk_editable_set_text (GTK_EDITABLE (self->username_entry),
                           self->request_data->username);
  } else {
    gtk_widget_set_visible (self->username_entry, FALSE);
  }

  gtk_editable_set_text (GTK_EDITABLE (self->password_entry),
                         self->request_data->password);
}

static void
ephy_password_popover_finalize (GObject *object)
{
  EphyPasswordPopover *popover = EPHY_PASSWORD_POPOVER (object);

  ephy_password_request_data_free (popover->request_data);

  G_OBJECT_CLASS (ephy_password_popover_parent_class)->finalize (object);
}

static void
ephy_password_popover_class_init (EphyPasswordPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ephy_password_popover_get_property;
  object_class->set_property = ephy_password_popover_set_property;
  object_class->constructed = ephy_password_popover_constructed;
  object_class->finalize = ephy_password_popover_finalize;

  obj_properties[PROP_REQUEST_DATA] =
    g_param_spec_pointer ("request-data", "", "",
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  /**
   * EphyPasswordPopover::response:
   * @popover: the object on which the signal is emitted
   *
   * Emitted when the user responses to the popover prompt
   *
   */
  signals[RESPONSE] = g_signal_new ("response", G_OBJECT_CLASS_TYPE (klass),
                                    G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                                    0, NULL, NULL, NULL,
                                    G_TYPE_NONE,
                                    0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/password-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyPasswordPopover, username_entry);
  gtk_widget_class_bind_template_child (widget_class, EphyPasswordPopover, password_entry);

  gtk_widget_class_bind_template_callback (widget_class, on_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_password_save);
  gtk_widget_class_bind_template_callback (widget_class, on_password_never);
}

static void
ephy_password_popover_init (EphyPasswordPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

EphyPasswordPopover *
ephy_password_popover_new (EphyPasswordRequestData *request_data)
{
  return g_object_new (EPHY_TYPE_PASSWORD_POPOVER,
                       "request-data", request_data,
                       NULL);
}
