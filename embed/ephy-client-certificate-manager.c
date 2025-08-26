/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2024 Jan-Michael Brummer <jan-michael.brummer1@volkswagen.de>
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

#include "ephy-client-certificate-manager.h"

#define GCK_API_SUBJECT_TO_CHANGE
#include <gck/gck.h>
#define GCR_API_SUBJECT_TO_CHANGE
#include <gcr/gcr.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <adwaita.h>

#include "ephy-web-view.h"

struct EphyClientCertificateManager {
  WebKitWebView *web_view;
  WebKitAuthenticationRequest *request;
  GckSession *session;
  GList *certificates;
  GCancellable *cancellable;
  GList *objects;
  char *password;
  char *current_label;
  WebKitCredentialPersistence persistence;
};

typedef struct {
  char *label;
  GckSlot *slot;
} EphyClientCertificate;

static void process_next_object (EphyClientCertificateManager *self);

static EphyClientCertificate *
ephy_client_certificate_new (char    *label,
                             GckSlot *slot)
{
  EphyClientCertificate *certificate = g_new0 (EphyClientCertificate, 1);

  certificate->label = g_strdup (label);
  certificate->slot = g_object_ref (slot);

  return certificate;
}

static void
ephy_client_certificate_free (EphyClientCertificate *certificate)
{
  g_clear_pointer (&certificate->label, g_free);
  g_clear_object (&certificate->slot);
  g_free (certificate);
}

void
ephy_client_certificate_manager_free (EphyClientCertificateManager *self)
{
  g_cancellable_cancel (self->cancellable);
  g_clear_pointer (&self->password, g_free);
  g_clear_pointer (&self->current_label, g_free);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->web_view);
  g_clear_object (&self->request);
  g_clear_object (&self->session);
  g_clear_list (&self->certificates, (GDestroyNotify)ephy_client_certificate_free);
  g_clear_list (&self->objects, (GDestroyNotify)g_object_unref);
  g_free (self);
}

static void
cancel_authentication (EphyClientCertificateManager *self)
{
  g_autoptr (WebKitCredential) credential = NULL;
  credential = webkit_credential_new (" ", "", WEBKIT_CREDENTIAL_PERSISTENCE_NONE);
  webkit_authentication_request_authenticate (self->request, credential);
}

static gboolean
is_this_a_slot_nobody_loves (GckSlot *slot)
{
  GckSlotInfo *slot_info;

  slot_info = gck_slot_get_info (slot);

  /* The p11-kit CA trusts do use their filesystem paths for description. */
  if (g_str_has_prefix (slot_info->slot_description, "/"))
    return TRUE;

  if (g_strcmp0 (slot_info->slot_description, "SSH Keys") == 0
      || g_strcmp0 (slot_info->slot_description, "Secret Store") == 0
      || g_strcmp0 (slot_info->slot_description, "User Key Storage") == 0)
    return TRUE;

  return FALSE;
}

static void
on_session_logout (GObject      *obj,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  GckSession *session = GCK_SESSION (obj);

  if (!gck_session_logout_finish (session, res, &error)) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Error during client certificate session logout: %s", error->message);
    return;
  }
}

static void
object_details_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  EphyClientCertificateManager *self = user_data;
  GckObject *object = GCK_OBJECT (source_object);
  g_autoptr (GckAttributes) attrs = NULL;
  const GckAttribute *attr;
  const GckAttribute *attr_label;
  CK_OBJECT_CLASS cka_class;
  g_autoptr (GError) error = NULL;

  attrs = gck_object_get_finish (object, res, &error);
  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Error getting PKCS #11 object attributes: %s", error->message);

    process_next_object (self);
    return;
  }

  if (!gck_attributes_find_ulong (attrs, CKA_CLASS, &cka_class)) {
    g_warning ("Skipping a PKCS #11 object without CKA_CLASS");
    process_next_object (self);
    return;
  }

  if (cka_class != CKO_CERTIFICATE) {
    process_next_object (self);
    return;
  }

  attr = gck_attributes_find (attrs, CKA_VALUE);
  attr_label = gck_attributes_find (attrs, CKA_LABEL);
  if (attr && attr->value && attr->length) {
    g_autoptr (GckAttributes) attributes = NULL;
    GckBuilder builder = GCK_BUILDER_INIT;
    GckUriData uri_data = { 0, };
    g_autofree char *uri = NULL;
    g_autofree char *cert = NULL;
    g_autofree char *priv = NULL;
    g_autoptr (GTlsCertificate) tls_cert = NULL;
    g_autoptr (WebKitCredential) credential = NULL;
    g_autofree char *label = g_strndup ((char *)attr_label->value, attr_label->length);

    gck_builder_add_string (&builder, CKA_LABEL, label);
    attributes = gck_builder_end (&builder);

    uri_data.attributes = attributes;
    uri_data.token_info = gck_slot_get_token_info (gck_session_get_slot (self->session));
    uri = gck_uri_data_build (&uri_data, GCK_URI_FOR_OBJECT_ON_TOKEN);

    cert = g_strconcat (uri, ";type=cert", NULL);
    priv = g_strconcat (uri, ";type=private", NULL);

    tls_cert = g_tls_certificate_new_from_pkcs11_uris (cert, priv, &error);
    if (error) {
      /* Do not log an error here as 'cert' and 'priv' does not need to
       * match. That's just guessing here.
       */
      g_clear_error (&error);

      process_next_object (self);
      return;
    }

    credential = webkit_credential_new_for_certificate (tls_cert, self->persistence);
    webkit_authentication_request_authenticate (self->request, credential);
    gck_session_logout_async (self->session, self->cancellable, on_session_logout, self);
  } else {
    process_next_object (self);
  }
}

static void
process_next_object (EphyClientCertificateManager *self)
{
  GckObject *object;
  const gulong attr_types[] = {
    CKA_ID, CKA_LABEL, CKA_ISSUER,
    CKA_VALUE, CKA_CLASS
  };

  if (!self->objects) {
    cancel_authentication (self);
    gck_session_logout_async (self->session, self->cancellable, on_session_logout, self);
    return;
  }

  object = self->objects->data;
  self->objects = g_list_remove (self->objects, object);

  gck_object_get_async (object, attr_types, sizeof (attr_types) / sizeof (attr_types[0]), self->cancellable, object_details_cb, self);
}

static void
next_object_cb (GObject      *obj,
                GAsyncResult *res,
                gpointer      user_data)
{
  EphyClientCertificateManager *self = user_data;
  GckEnumerator *enm = GCK_ENUMERATOR (obj);
  g_autoptr (GError) error = NULL;

  self->objects = gck_enumerator_next_finish (enm, res, &error);
  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Error getting client certificate object: %s", error->message);
    cancel_authentication (self);
    return;
  }

  process_next_object (self);
}

static void
logged_in_cb (GObject      *obj,
              GAsyncResult *res,
              gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  GckSession *session = GCK_SESSION (obj);
  EphyClientCertificateManager *self = user_data;
  g_autoptr (GckEnumerator) enm = NULL;
  g_autoptr (GckAttributes) attributes = NULL;

  if (!gck_session_login_finish (session, res, &error)) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Error during client certificate session login: %s", error->message);
    cancel_authentication (self);
    return;
  }

  attributes = gck_attributes_new_empty (GCK_INVALID);
  enm = gck_session_enumerate_objects (session, attributes);
  gck_enumerator_next_async (g_steal_pointer (&enm), -1, self->cancellable, next_object_cb, self);
}

static void
certificate_pin_response (AdwAlertDialog *dialog,
                          char           *response,
                          gpointer        user_data)
{
  EphyClientCertificateManager *self = user_data;
  GtkWidget *entry = adw_alert_dialog_get_extra_child (dialog);
  const char *password = gtk_editable_get_text (GTK_EDITABLE (entry));

  if (strcmp (response, "cancel") == 0) {
    cancel_authentication (self);
    return;
  }

  g_assert (!self->password);
  self->password = g_strdup (password);
  gck_session_login_async (self->session, CKU_USER, (guint8 *)self->password, strlen (self->password), self->cancellable, logged_in_cb, self);
}

static void
session_opened_cb (GObject      *obj,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  EphyClientCertificateManager *self = user_data;
  GckSlot *slot = GCK_SLOT (obj);
  g_autoptr (GError) error = NULL;
  g_autoptr (GList) modules = NULL;
  g_autoptr (GList) slots = NULL;
  AdwDialog *dialog;
  GtkWidget *entry;
  g_autofree char *body = NULL;
  GckTokenInfo *info;

  self->session = gck_slot_open_session_finish (slot, res, &error);
  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Could not open client certificate session: %s", error->message);
    cancel_authentication (self);
    return;
  }

  dialog = adw_alert_dialog_new (_("PIN required"), NULL);

  info = gck_slot_get_token_info (slot);
  body = g_strdup_printf (_("Please enter PIN for %s, to authenticate at %s:%d."),
                          info->label,
                          webkit_authentication_request_get_host (self->request),
                          webkit_authentication_request_get_port (self->request));

  adw_alert_dialog_format_body (ADW_ALERT_DIALOG (dialog), "%s", body);

  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                  "cancel", _("_Cancel"),
                                  "login", _("_Login"),
                                  NULL);

  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "login", ADW_RESPONSE_SUGGESTED);

  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "login");
  adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "cancel");

  entry = adw_password_entry_row_new ();
  gtk_widget_add_css_class (entry, "card");
  gtk_text_set_activates_default (GTK_TEXT (gtk_editable_get_delegate (GTK_EDITABLE (entry))), TRUE);

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (entry), "PIN");
  adw_alert_dialog_set_extra_child (ADW_ALERT_DIALOG (dialog), entry);

  g_signal_connect (dialog, "response", G_CALLBACK (certificate_pin_response), self);

  adw_dialog_present (dialog, GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self->web_view))));
  gtk_widget_grab_focus (entry);
}

static void
certificate_selection_dialog_response_cb (AdwAlertDialog *dialog,
                                          char           *response,
                                          gpointer        user_data)
{
  EphyClientCertificateManager *self = user_data;
  g_autoptr (GError) error = NULL;
  GckSlot *slot = NULL;

  if (strcmp (response, "cancel") == 0) {
    cancel_authentication (self);
    return;
  }

  for (GList *iter = self->certificates; iter && iter->data; iter = iter->next) {
    EphyClientCertificate *cert = iter->data;

    if (g_strcmp0 (cert->label, self->current_label) == 0) {
      slot = cert->slot;
      break;
    }
  }

  if (!slot) {
    g_warning ("Unknown certificate label selected, abort!");
    return;
  }

  gck_slot_open_session_async (slot, GCK_SESSION_READ_ONLY, NULL, self->cancellable, session_opened_cb, self);
}

static void
on_radio_button_toggled (GtkWidget *button,
                         gpointer   user_data)
{
  EphyClientCertificateManager *self = user_data;

  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (button))) {
    GtkWidget *row = gtk_widget_get_ancestor (GTK_WIDGET (button), ADW_TYPE_ACTION_ROW);
    const char *label = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));

    g_clear_pointer (&self->current_label, g_free);
    self->current_label = g_strdup (label);
  }
}

static void
on_remember_decision_selected (AdwComboRow                  *row,
                               GParamSpec                   *psepc,
                               EphyClientCertificateManager *self)
{
  WebKitCredentialPersistence persistence;

  switch (adw_combo_row_get_selected (row)) {
    default:
    case 0:
      persistence = WEBKIT_CREDENTIAL_PERSISTENCE_NONE;
      break;
    case 1:
      persistence = WEBKIT_CREDENTIAL_PERSISTENCE_FOR_SESSION;
      break;
    case 2:
      persistence = WEBKIT_CREDENTIAL_PERSISTENCE_PERMANENT;
      break;
  }

  self->persistence = persistence;
}

static void
certificate_selection_dialog (EphyClientCertificateManager *self)
{
  AdwDialog *dialog;
  GtkWidget *box;
  GtkWidget *listbox;
  GtkWidget *option_listbox;
  GtkWidget *check_button_group = NULL;
  GtkWidget *remember_decision_combo_row;
  GtkStringList *list;
  g_autofree char *body = NULL;
  const char *realm = webkit_authentication_request_get_realm (self->request);

  /* In case there are no certificates, retrigger authentication */
  if (g_list_length (self->certificates) == 0) {
    cancel_authentication (self);
    return;
  }

  dialog = adw_alert_dialog_new (_("Select certificate"), NULL);
  gtk_widget_set_size_request (GTK_WIDGET (dialog), 360, -1);

  if (strlen (realm) > 0)
    body = g_strdup_printf (_("The website %s:%d requests that you provide a certificate for authentication for %s."),
                            webkit_authentication_request_get_host (self->request),
                            webkit_authentication_request_get_port (self->request),
                            realm);
  else
    body = g_strdup_printf (_("The website %s:%d requests that you provide a certificate for authentication."),
                            webkit_authentication_request_get_host (self->request),
                            webkit_authentication_request_get_port (self->request));

  adw_alert_dialog_format_body (ADW_ALERT_DIALOG (dialog), "%s", body);
  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                  "cancel", _("_Cancel"),
                                  "select", _("_Select"),
                                  NULL);

  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "select", ADW_RESPONSE_SUGGESTED);

  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "select");
  adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "cancel");

  listbox = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (listbox), GTK_SELECTION_NONE);
  gtk_widget_add_css_class (listbox, "content");

  for (GList *iter = self->certificates; iter; iter = iter->next) {
    EphyClientCertificate *certificate = iter->data;
    GtkWidget *row;
    GtkWidget *check_button;

    row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), certificate->label);

    check_button = gtk_check_button_new ();
    gtk_widget_set_valign (check_button, GTK_ALIGN_CENTER);
    g_signal_connect (G_OBJECT (check_button), "toggled", G_CALLBACK (on_radio_button_toggled), self);
    adw_action_row_add_prefix (ADW_ACTION_ROW (row), GTK_WIDGET (check_button));
    adw_action_row_set_activatable_widget (ADW_ACTION_ROW (row), check_button);
    gtk_check_button_set_group (GTK_CHECK_BUTTON (check_button), GTK_CHECK_BUTTON (check_button_group));

    if (!check_button_group) {
      check_button_group = check_button;
      gtk_check_button_set_active (GTK_CHECK_BUTTON (check_button), TRUE);
    }

    gtk_list_box_append (GTK_LIST_BOX (listbox), row);
  }

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_box_append (GTK_BOX (box), listbox);

  option_listbox = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (option_listbox), GTK_SELECTION_NONE);
  gtk_widget_add_css_class (option_listbox, "content");
  remember_decision_combo_row = adw_combo_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (remember_decision_combo_row), _("Remember Decision"));

  list = gtk_string_list_new (NULL);
  gtk_string_list_append (list, _("Never"));
  gtk_string_list_append (list, _("For Session"));
  gtk_string_list_append (list, _("Permanently"));

  adw_combo_row_set_model (ADW_COMBO_ROW (remember_decision_combo_row), G_LIST_MODEL (list));
  g_signal_connect (remember_decision_combo_row, "notify::selected", G_CALLBACK (on_remember_decision_selected), self);
  adw_combo_row_set_selected (ADW_COMBO_ROW (remember_decision_combo_row), 1);
  gtk_list_box_append (GTK_LIST_BOX (option_listbox), remember_decision_combo_row);
  gtk_box_append (GTK_BOX (box), option_listbox);

  adw_alert_dialog_set_extra_child (ADW_ALERT_DIALOG (dialog), box);

  g_signal_connect (dialog, "response", G_CALLBACK (certificate_selection_dialog_response_cb), self);

  adw_dialog_present (dialog, GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self->web_view))));
}

static void
modules_initialized_cb (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  EphyClientCertificateManager *self = user_data;
  g_autoptr (GError) error = NULL;
  g_autolist (GckModule) modules = NULL;
  g_autolist (GckSlot) slots = NULL;

  modules = gck_modules_initialize_registered_finish (res, &error);
  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Could not initialize registered PKCS #11 modules: %s", error->message);
    cancel_authentication (self);
    return;
  }

  slots = gck_modules_get_slots (modules, TRUE);
  for (GList *iter = slots; iter && iter->data; iter = iter->next) {
    GckSlot *slot = GCK_SLOT (iter->data);
    g_autoptr (GckTokenInfo) info = NULL;
    g_autofree char *label = NULL;

    if (is_this_a_slot_nobody_loves (slot))
      continue;

    info = gck_slot_get_token_info (slot);
    if (!info) {
      /* This happens when the slot has no token inserted. */
      continue;
    }

    if ((info->flags & CKF_TOKEN_INITIALIZED) == 0)
      continue;

    if (info->label && *info->label) {
      label = g_strdup (info->label);
    } else if (info->model && *info->model) {
      g_info ("The client token doesn't have a valid label, falling back to model.");
      label = g_strdup (info->model);
    } else {
      g_info ("The client token has neither valid label nor model, using Unknown.");
      label = g_strdup ("(Unknown)");
    }

    self->certificates = g_list_append (self->certificates, ephy_client_certificate_new (label, slot));
  }

  /* Time to create user dialog */
  certificate_selection_dialog (self);
}

EphyClientCertificateManager *
ephy_client_certificate_manager_request_certificate (WebKitWebView               *web_view,
                                                     WebKitAuthenticationRequest *request)
{
  EphyClientCertificateManager *self = g_new0 (EphyClientCertificateManager, 1);

  self->web_view = g_object_ref (web_view);
  self->request = g_object_ref (request);
  self->cancellable = g_cancellable_new ();

  gck_modules_initialize_registered_async (self->cancellable, modules_initialized_cb, self);

  return self;
}

void
ephy_client_certificate_manager_request_certificate_pin (EphyClientCertificateManager *self,
                                                         WebKitWebView                *web_view,
                                                         WebKitAuthenticationRequest  *request)
{
  g_autoptr (WebKitCredential) credential = NULL;

  if (g_strcmp0 (webkit_web_view_get_uri (self->web_view), webkit_web_view_get_uri (web_view)) == 0 && self->password) {
    credential = webkit_credential_new_for_certificate_pin (self->password, self->persistence);
    webkit_authentication_request_authenticate (request, credential);
  } else {
    cancel_authentication (self);
  }
}
