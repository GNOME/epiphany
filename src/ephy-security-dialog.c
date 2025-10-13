/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2014, 2015 Igalia S.L.
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
#include "ephy-security-dialog.h"

#include <glib/gi18n.h>

#include "ephy-certificate-dialog.h"
#include "ephy-embed-shell.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-permissions-manager.h"
#include "ephy-settings.h"
#include "ephy-uri-helpers.h"

/**
 * SECTION:ephy-security-dialog
 * @short_description: A dialog to show basic TLS connection information
 *
 * #EphySecurityDialog shows basic information about a TLS connection
 * and allows opening #EphyCertificateDialog for more detailed information. It
 * can also be used to show that a connection does not use TLS at all.
 */

enum {
  PROP_0,
  PROP_ADDRESS,
  PROP_CERTIFICATE,
  PROP_SECURITY_LEVEL,
  PROP_TLS_ERRORS,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

struct _EphySecurityDialog {
  AdwDialog parent_instance;
  char *address;
  char *decoded_hostname;
  guint permission_pos;
  GtkWidget *status_page;
  GtkWidget *certificate_button;
  GtkWidget *ad_combobox;
  GtkWidget *notification_combobox;
  GtkWidget *save_password_combobox;
  GtkWidget *access_location_combobox;
  GtkWidget *access_microphone_combobox;
  GtkWidget *access_webcam_combobox;
  GtkWidget *access_display_combobox;
  GtkWidget *autoplay_combobox;
  GtkWidget *listbox;
  GTlsCertificate *certificate;
  GTlsCertificateFlags tls_errors;
  EphySecurityLevel security_level;
};

G_DEFINE_FINAL_TYPE (EphySecurityDialog, ephy_security_dialog, ADW_TYPE_DIALOG)

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static void
set_permission_ads_combobox_state (EphyPermissionsManager *permissions_manager,
                                   gint                    permission_id,
                                   gchar                  *origin,
                                   GtkWidget              *widget)
{
  GSettings *web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);
  EphyPermission permission;

  permission = ephy_permissions_manager_get_permission (permissions_manager,
                                                        permission_id,
                                                        origin);

  switch (permission) {
    case EPHY_PERMISSION_UNDECIDED:
      adw_combo_row_set_selected (ADW_COMBO_ROW (widget),
                                  g_settings_get_boolean (web_settings, EPHY_PREFS_WEB_ENABLE_ADBLOCK));
      break;
    case EPHY_PERMISSION_DENY:
      adw_combo_row_set_selected (ADW_COMBO_ROW (widget), 1);
      break;
    case EPHY_PERMISSION_PERMIT:
      adw_combo_row_set_selected (ADW_COMBO_ROW (widget), 0);
      break;
  }
}

static void
set_permission_combobox_state (EphyPermissionsManager *permissions_manager,
                               gint                    permission_id,
                               gchar                  *origin,
                               GtkWidget              *widget)
{
  EphyPermission permission;

  permission = ephy_permissions_manager_get_permission (permissions_manager,
                                                        permission_id,
                                                        origin);

  switch (permission) {
    case EPHY_PERMISSION_PERMIT:
      adw_combo_row_set_selected (ADW_COMBO_ROW (widget), 0);
      break;
    case EPHY_PERMISSION_DENY:
      adw_combo_row_set_selected (ADW_COMBO_ROW (widget), 1);
      break;
    case EPHY_PERMISSION_UNDECIDED:
      adw_combo_row_set_selected (ADW_COMBO_ROW (widget), 2);
      break;
  }
}
G_GNUC_END_IGNORE_DEPRECATIONS

static void
ephy_security_dialog_set_address (EphySecurityDialog *dialog,
                                  const char         *address)
{
  EphyPermissionsManager *permissions_manager;
  g_autoptr (GUri) uri = NULL;
  g_autofree char *decoded_url = NULL;
  g_autofree char *origin = NULL;
  g_autofree char *uri_text = NULL;

  uri = g_uri_parse (address, G_URI_FLAGS_PARSE_RELAXED, NULL);
  /* Label when clicking the lock icon on a secure page. %s is the website's hostname. */
  adw_status_page_set_title (ADW_STATUS_PAGE (dialog->status_page), g_uri_get_host (uri));

  dialog->address = g_strdup (address);

  decoded_url = ephy_uri_decode (address);
  if (!decoded_url)
    decoded_url = g_strdup (address);
  dialog->decoded_hostname = ephy_uri_get_decoded_host (decoded_url);

  origin = ephy_uri_to_security_origin (address);
  if (!origin)
    return;

  permissions_manager = ephy_embed_shell_get_permissions_manager (ephy_embed_shell_get_default ());
  set_permission_ads_combobox_state (permissions_manager, EPHY_PERMISSION_TYPE_SHOW_ADS, origin, dialog->ad_combobox);
  set_permission_combobox_state (permissions_manager, EPHY_PERMISSION_TYPE_SHOW_NOTIFICATIONS, origin, dialog->notification_combobox);
  set_permission_combobox_state (permissions_manager, EPHY_PERMISSION_TYPE_SAVE_PASSWORD, origin, dialog->save_password_combobox);
  set_permission_combobox_state (permissions_manager, EPHY_PERMISSION_TYPE_ACCESS_LOCATION, origin, dialog->access_location_combobox);
  set_permission_combobox_state (permissions_manager, EPHY_PERMISSION_TYPE_ACCESS_MICROPHONE, origin, dialog->access_microphone_combobox);
  set_permission_combobox_state (permissions_manager, EPHY_PERMISSION_TYPE_ACCESS_WEBCAM, origin, dialog->access_webcam_combobox);
  set_permission_combobox_state (permissions_manager, EPHY_PERMISSION_TYPE_ACCESS_DISPLAY, origin, dialog->access_display_combobox);
  set_permission_combobox_state (permissions_manager, EPHY_PERMISSION_TYPE_AUTOPLAY_POLICY, origin, dialog->autoplay_combobox);
}

static void
ephy_security_dialog_set_certificate (EphySecurityDialog *dialog,
                                      GTlsCertificate    *certificate)
{
  if (certificate)
    dialog->certificate = g_object_ref (certificate);
}

static void
ephy_security_dialog_set_security_level (EphySecurityDialog *dialog,
                                         EphySecurityLevel   security_level)
{
  g_autofree gchar *address_text = NULL;
  g_autofree gchar *label_text = NULL;

  dialog->security_level = security_level;

  address_text = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>", dialog->decoded_hostname);
  adw_status_page_set_title (ADW_STATUS_PAGE (dialog->status_page), dialog->decoded_hostname);

  switch (security_level) {
    case EPHY_SECURITY_LEVEL_UNACCEPTABLE_CERTIFICATE:
      /* Label in certificate dialog when site is untrusted. %s is a URL. */
      label_text = g_strdup_printf (_("This web site’s digital identification is not trusted. "
                                      "You may have connected to an attacker pretending to be %s"),
                                    address_text);
      adw_status_page_set_description (ADW_STATUS_PAGE (dialog->status_page), label_text);
      break;
    case EPHY_SECURITY_LEVEL_NO_SECURITY:
      /* Label in certificate dialog when site uses HTTP. %s is a URL. */
      label_text = g_strdup_printf (_("This site has no security. An attacker could see any information "
                                      "you send, or control the content that you see"));
      adw_status_page_set_description (ADW_STATUS_PAGE (dialog->status_page), label_text);
      break;
    case EPHY_SECURITY_LEVEL_MIXED_CONTENT:
      adw_status_page_set_description (ADW_STATUS_PAGE (dialog->status_page),
                                       /* Label in certificate dialog when site sends mixed content. */
                                       _("This web site did not properly secure your connection"));
      break;
    case EPHY_SECURITY_LEVEL_STRONG_SECURITY:
      adw_status_page_set_description (ADW_STATUS_PAGE (dialog->status_page),
                                       /* Label in certificate dialog on secure sites. */
                                       _("Your connection seems to be secure"));
      break;
    case EPHY_SECURITY_LEVEL_TO_BE_DETERMINED:
    case EPHY_SECURITY_LEVEL_LOCAL_PAGE:
    default:
      g_assert_not_reached ();
  }

  adw_status_page_set_icon_name (ADW_STATUS_PAGE (dialog->status_page), ephy_security_level_to_icon_name (security_level));
}

static void
certificate_button_clicked_cb (GtkButton *button,
                               gpointer   user_data)
{
  EphySecurityDialog *self = EPHY_SECURITY_DIALOG (user_data);
  AdwDialog *dialog;

  dialog = ephy_certificate_dialog_new (self->address,
                                        self->certificate,
                                        self->tls_errors,
                                        self->security_level);

  adw_dialog_present (dialog, GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self))));
}

static void
ephy_security_dialog_constructed (GObject *object)
{
  EphySecurityDialog *dialog = EPHY_SECURITY_DIALOG (object);

  G_OBJECT_CLASS (ephy_security_dialog_parent_class)->constructed (object);

  if (!dialog->certificate) {
    gtk_widget_set_visible (dialog->certificate_button, FALSE);
    return;
  }
}

static void
ephy_security_dialog_dispose (GObject *object)
{
  EphySecurityDialog *dialog = EPHY_SECURITY_DIALOG (object);

  g_clear_object (&dialog->certificate);

  G_OBJECT_CLASS (ephy_security_dialog_parent_class)->dispose (object);
}

static void
ephy_security_dialog_finalize (GObject *object)
{
  EphySecurityDialog *dialog = EPHY_SECURITY_DIALOG (object);

  g_free (dialog->address);
  g_free (dialog->decoded_hostname);

  G_OBJECT_CLASS (ephy_security_dialog_parent_class)->finalize (object);
}

static void
ephy_security_dialog_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  EphySecurityDialog *dialog = EPHY_SECURITY_DIALOG (object);

  switch (prop_id) {
    case PROP_ADDRESS:
      ephy_security_dialog_set_address (dialog, g_value_get_string (value));
      break;
    case PROP_CERTIFICATE:
      ephy_security_dialog_set_certificate (dialog, g_value_get_object (value));
      break;
    case PROP_SECURITY_LEVEL:
      ephy_security_dialog_set_security_level (dialog, g_value_get_enum (value));
      break;
    case PROP_TLS_ERRORS:
      dialog->tls_errors = g_value_get_flags (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_security_dialog_class_init (EphySecurityDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ephy_security_dialog_constructed;
  object_class->dispose = ephy_security_dialog_dispose;
  object_class->finalize = ephy_security_dialog_finalize;
  object_class->set_property = ephy_security_dialog_set_property;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/security-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, EphySecurityDialog, status_page);
  gtk_widget_class_bind_template_child (widget_class, EphySecurityDialog, listbox);
  gtk_widget_class_bind_template_child (widget_class, EphySecurityDialog, certificate_button);

  gtk_widget_class_bind_template_callback (widget_class, certificate_button_clicked_cb);

  /**
   * EphySecurityDialog:address:
   *
   * The address of the website.
   */
  obj_properties[PROP_ADDRESS] =
    g_param_spec_string ("address",
                         NULL, NULL,
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * EphySecurityDialog:certificate:
   *
   * The certificate of the website.
   */
  obj_properties[PROP_CERTIFICATE] =
    g_param_spec_object ("certificate",
                         NULL, NULL,
                         G_TYPE_TLS_CERTIFICATE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * EphySecurityDialog:tls-errors:
   *
   * Indicates issues with the security of the website.
   */
  obj_properties[PROP_TLS_ERRORS] =
    g_param_spec_flags ("tls-errors",
                        NULL, NULL,
                        G_TYPE_TLS_CERTIFICATE_FLAGS,
                        0,
                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * EphySecurityDialog:security-level:
   *
   * The state of the lock displayed in the address bar.
   */
  obj_properties[PROP_SECURITY_LEVEL] =
    g_param_spec_enum ("security-level",
                       NULL, NULL,
                       EPHY_TYPE_SECURITY_LEVEL,
                       EPHY_SECURITY_LEVEL_TO_BE_DETERMINED,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static void
on_ad_combobox_changed (AdwComboRow        *widget,
                        GParamSpec         *pspec,
                        EphySecurityDialog *dialog)
{
  GSettings *web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);
  EphyPermissionsManager *permissions_manager;
  EphyPermission permission = EPHY_PERMISSION_UNDECIDED;
  gboolean global_flag = g_settings_get_boolean (web_settings, EPHY_PREFS_WEB_ENABLE_ADBLOCK);
  g_autofree gchar *origin = NULL;
  gboolean state = adw_combo_row_get_selected (widget) == 1;

  origin = ephy_uri_to_security_origin (dialog->address);
  if (!origin)
    return;

  permissions_manager = ephy_embed_shell_get_permissions_manager (ephy_embed_shell_get_default ());

  if (global_flag != state)
    permission = state ? EPHY_PERMISSION_DENY : EPHY_PERMISSION_PERMIT;

  ephy_permissions_manager_set_permission (permissions_manager,
                                           EPHY_PERMISSION_TYPE_SHOW_ADS,
                                           origin,
                                           permission);
}
G_GNUC_END_IGNORE_DEPRECATIONS

static void
handle_permission_combobox_changed (EphySecurityDialog *dialog,
                                    gint                action,
                                    EphyPermissionType  permission_type)
{
  EphyPermissionsManager *permissions_manager;
  EphyPermission permission;
  g_autofree gchar *origin = NULL;

  origin = ephy_uri_to_security_origin (dialog->address);
  if (!origin)
    return;

  permissions_manager = ephy_embed_shell_get_permissions_manager (ephy_embed_shell_get_default ());

  switch (action) {
    case 0:
      permission = EPHY_PERMISSION_PERMIT;
      break;
    default:
    case 1:
      permission = EPHY_PERMISSION_DENY;
      break;
    case 2:
      permission = EPHY_PERMISSION_UNDECIDED;
      break;
  }

  ephy_permissions_manager_set_permission (permissions_manager,
                                           permission_type,
                                           origin,
                                           permission);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static void
on_notification_combobox_changed (AdwComboRow        *row,
                                  GParamSpec         *psepc,
                                  EphySecurityDialog *dialog)
{
  handle_permission_combobox_changed (dialog, adw_combo_row_get_selected (row), EPHY_PERMISSION_TYPE_SHOW_NOTIFICATIONS);
}

static void
on_save_password_combobox_changed (AdwComboRow        *row,
                                   GParamSpec         *psepc,
                                   EphySecurityDialog *dialog)
{
  handle_permission_combobox_changed (dialog, adw_combo_row_get_selected (row), EPHY_PERMISSION_TYPE_SAVE_PASSWORD);
}

static void
on_access_location_combobox_changed (AdwComboRow        *row,
                                     GParamSpec         *psepc,
                                     EphySecurityDialog *dialog)
{
  handle_permission_combobox_changed (dialog, adw_combo_row_get_selected (row), EPHY_PERMISSION_TYPE_ACCESS_LOCATION);
}

static void
on_access_microphone_combobox_changed (AdwComboRow        *row,
                                       GParamSpec         *psepc,
                                       EphySecurityDialog *dialog)
{
  handle_permission_combobox_changed (dialog, adw_combo_row_get_selected (row), EPHY_PERMISSION_TYPE_ACCESS_MICROPHONE);
}

static void
on_access_webcam_combobox_changed (AdwComboRow        *row,
                                   GParamSpec         *psepc,
                                   EphySecurityDialog *dialog)
{
  handle_permission_combobox_changed (dialog, adw_combo_row_get_selected (row), EPHY_PERMISSION_TYPE_ACCESS_WEBCAM);
}

static void
on_access_display_combobox_changed (AdwComboRow        *row,
                                    GParamSpec         *psepc,
                                    EphySecurityDialog *dialog)
{
  handle_permission_combobox_changed (dialog, adw_combo_row_get_selected (row), EPHY_PERMISSION_TYPE_ACCESS_DISPLAY);
}

static void
on_autoplay_policy_combobox_changed (AdwComboRow        *row,
                                     GParamSpec         *psepc,
                                     EphySecurityDialog *dialog)
{
  handle_permission_combobox_changed (dialog, adw_combo_row_get_selected (row), EPHY_PERMISSION_TYPE_AUTOPLAY_POLICY);
}

static GtkWidget *
add_permission_combobox (EphySecurityDialog *dialog,
                         const gchar        *name,
                         gpointer            callback,
                         GtkSizeGroup       *size_group,
                         gboolean            no_ask,
                         const gchar        *third_option_name)
{
  GtkWidget *widget;
  GtkStringList *list;

  widget = adw_combo_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (widget), name);

  list = gtk_string_list_new (NULL);
  gtk_string_list_append (list, _("Allow"));
  gtk_string_list_append (list, _("Deny"));
  if (!no_ask) {
    const gchar *name = !third_option_name ? _("Ask") : third_option_name;
    gtk_string_list_append (list, _(name));
  }

  adw_combo_row_set_model (ADW_COMBO_ROW (widget), G_LIST_MODEL (list));

  g_signal_connect (widget, "notify::selected", G_CALLBACK (callback), dialog);
  gtk_list_box_append (GTK_LIST_BOX (dialog->listbox), widget);

  return widget;
}
G_GNUC_END_IGNORE_DEPRECATIONS

static void
ephy_security_dialog_init (EphySecurityDialog *dialog)
{
  g_autoptr (GtkSizeGroup) combo_box_size_group = NULL;

  gtk_widget_init_template (GTK_WIDGET (dialog));

  dialog->permission_pos = 0;
  combo_box_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  /* TRANSLATORS: This is a permission toggle in the Security & Permissions dialog. */
  dialog->ad_combobox = add_permission_combobox (dialog, _("Advertisements"), on_ad_combobox_changed, combo_box_size_group, TRUE, NULL);
  /* TRANSLATORS: This is a permission toggle in the Security & Permissions dialog. */
  dialog->notification_combobox = add_permission_combobox (dialog, _("Notifications"), on_notification_combobox_changed, combo_box_size_group, FALSE, NULL);
  /* TRANSLATORS: This is a permission toggle in the Security & Permissions dialog. */
  dialog->save_password_combobox = add_permission_combobox (dialog, _("Password Saving"), on_save_password_combobox_changed, combo_box_size_group, FALSE, NULL);
  /* TRANSLATORS: This is a permission toggle in the Security & Permissions dialog. */
  dialog->access_location_combobox = add_permission_combobox (dialog, _("Location Access"), on_access_location_combobox_changed, combo_box_size_group, FALSE, NULL);
  /* TRANSLATORS: This is a permission toggle in the Security & Permissions dialog. */
  dialog->access_microphone_combobox = add_permission_combobox (dialog, _("Microphone Access"), on_access_microphone_combobox_changed, combo_box_size_group, FALSE, NULL);
  /* TRANSLATORS: This is a permission toggle in the Security & Permissions dialog. */
  dialog->access_webcam_combobox = add_permission_combobox (dialog, _("Webcam Access"), on_access_webcam_combobox_changed, combo_box_size_group, FALSE, NULL);
  /* TRANSLATORS: This is a permission toggle in the Security & Permissions dialog. */
  dialog->access_display_combobox = add_permission_combobox (dialog, _("Display Access"), on_access_display_combobox_changed, combo_box_size_group, FALSE, NULL);
  /* TRANSLATORS: This is a permission toggle in the Security & Permissions dialog. */
  dialog->autoplay_combobox = add_permission_combobox (dialog, _("Media Autoplay"), on_autoplay_policy_combobox_changed, combo_box_size_group, FALSE, _("Without Sound"));
}

GtkWidget *
ephy_security_dialog_new (const char           *address,
                          GTlsCertificate      *certificate,
                          GTlsCertificateFlags  tls_errors,
                          EphySecurityLevel     security_level)
{
  g_assert (address);

  return GTK_WIDGET (g_object_new (EPHY_TYPE_SECURITY_DIALOG,
                                   "address", address,
                                   "certificate", certificate,
                                   "security-level", security_level,
                                   "tls-errors", tls_errors,
                                   NULL));
}
