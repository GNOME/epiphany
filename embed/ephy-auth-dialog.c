/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2020 Jan-Michael Brummer <jan.brummer@tabos.org>
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

#include <glib/gi18n.h>
#include <webkit2/webkit2.h>

#include "ephy-auth-dialog.h"

struct _EphyAuthDialog {
  GtkMessageDialog parent_instance;

  WebKitAuthenticationRequest *request;

  GtkWidget *username;
  GtkWidget *password;
  GtkWidget *remember;
};

G_DEFINE_TYPE (EphyAuthDialog, ephy_auth_dialog, GTK_TYPE_MESSAGE_DIALOG)

enum {
  PROP_0,
  PROP_REQUEST,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static void
ephy_auth_dialog_init (EphyAuthDialog *self)
{
}

static void
ephy_auth_dialog_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  /* no readable properties */
  g_assert_not_reached ();
}

static void
ephy_auth_dialog_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  EphyAuthDialog *dialog = EPHY_AUTH_DIALOG (object);

  switch (prop_id) {
    case PROP_REQUEST:
      dialog->request = (WebKitAuthenticationRequest *)g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_auth_dialog_response_cb (GtkDialog *dialog,
                              gint       response_id,
                              gpointer   user_data)
{
  EphyAuthDialog *self = EPHY_AUTH_DIALOG (dialog);
  EphyPasswordManager *manager = ephy_embed_shell_get_password_manager (EPHY_EMBED_SHELL (ephy_embed_shell_get_default ()));
  WebKitCredential *credential = NULL;

  if (response_id == GTK_RESPONSE_OK)
    credential = webkit_credential_new (gtk_entry_get_text (GTK_ENTRY (self->username)),
                                        gtk_entry_get_text (GTK_ENTRY (self->password)),
                                        WEBKIT_CREDENTIAL_PERSISTENCE_NONE);

  webkit_authentication_request_authenticate (self->request, credential);

  ephy_password_manager_save (manager,
                              webkit_authentication_request_get_host (self->request),
                              webkit_authentication_request_get_host (self->request),
                              gtk_entry_get_text (GTK_ENTRY (self->username)),
                              gtk_entry_get_text (GTK_ENTRY (self->password)),
                              "",
                              "",
                              TRUE);

  gtk_widget_destroy (GTK_WIDGET (self));
}

static void
ephy_auth_dialog_constructed (GObject *object)
{
  EphyAuthDialog *self = EPHY_AUTH_DIALOG (object);
  GtkWidget *content_area;
  GtkWidget *content_grid;
  GtkWidget *grid;
  GtkWidget *label;
  g_autofree char *realm_text = NULL;
  const char *realm;
  WebKitCredential *credential;

  G_OBJECT_CLASS (ephy_auth_dialog_parent_class)->constructed (object);

  credential = webkit_authentication_request_get_proposed_credential (self->request);

  gtk_window_set_title (GTK_WINDOW (self), _("Authentication Required"));

  content_grid = gtk_grid_new ();

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_grid_attach (GTK_GRID (content_grid), grid, 0, 0, 1, 1);

  realm = webkit_authentication_request_get_realm (self->request);
  if (realm && strlen (realm) > 0)
    realm_text = g_strdup_printf ("\n%s \"%s\"", _("The site says:"), realm);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (self),
                                            "%s %s:%d%s",
                                            _("Authentication required by"),
                                            webkit_authentication_request_get_host (self->request),
                                            webkit_authentication_request_get_port (self->request),
                                            realm_text ? realm_text : "");

  label = gtk_label_new (_("Username"));
  gtk_label_set_xalign (GTK_LABEL (label), 1);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), GTK_STYLE_CLASS_DIM_LABEL);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  self->username = gtk_entry_new ();
  if (credential)
    gtk_entry_set_text (GTK_ENTRY (self->username), webkit_credential_get_username (credential));

  gtk_widget_set_hexpand (self->username, TRUE);
  gtk_grid_attach (GTK_GRID (grid), self->username, 1, 0, 1, 1);

  label = gtk_label_new (_("Password"));
  gtk_label_set_xalign (GTK_LABEL (label), 1);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), GTK_STYLE_CLASS_DIM_LABEL);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

  self->password = gtk_entry_new ();
  if (credential)
    gtk_entry_set_text (GTK_ENTRY (self->username), webkit_credential_get_password (credential));

  gtk_entry_set_visibility (GTK_ENTRY (self->password), FALSE);
  gtk_widget_set_hexpand (self->password, TRUE);
  gtk_grid_attach (GTK_GRID (grid), self->password, 1, 1, 1, 1);

  content_area = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (self));
  gtk_container_add (GTK_CONTAINER (content_area), content_grid);

  gtk_window_set_position (GTK_WINDOW (self), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (self), TRUE);

  g_signal_connect (self, "response", G_CALLBACK (ephy_auth_dialog_response_cb), NULL);
}

static void
ephy_auth_dialog_class_init (EphyAuthDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ephy_auth_dialog_constructed;
  object_class->get_property = ephy_auth_dialog_get_property;
  object_class->set_property = ephy_auth_dialog_set_property;

  obj_properties[PROP_REQUEST] =
  g_param_spec_object ("request",
                       "WebKitAuthenticationRequest",
                       "WebKit Authentication Request",
                       WEBKIT_TYPE_AUTHENTICATION_REQUEST,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

GtkWidget *
ephy_auth_dialog_new (WebKitAuthenticationRequest *request)
{
  g_assert (request != NULL);

  return g_object_new (EPHY_TYPE_AUTH_DIALOG, "buttons", GTK_BUTTONS_OK_CANCEL, "request", request, NULL);
}

