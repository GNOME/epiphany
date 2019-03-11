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
#include "ephy-security-popover.h"

#include <glib/gi18n.h>
#include <libsoup/soup.h>

#include "ephy-certificate-dialog.h"
#include "ephy-embed-shell.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-permissions-manager.h"
#include "ephy-settings.h"
#include "ephy-uri-helpers.h"

/**
 * SECTION:ephy-security-popover
 * @short_description: A popover to show basic TLS connection information
 *
 * #EphySecurityPopover shows basic information about a TLS connection
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

struct _EphySecurityPopover {
  GtkPopover parent_instance;
  char *address;
  char *hostname;
  GtkWidget *lock_image;
  GtkWidget *host_label;
  GtkWidget *security_label;
  GtkWidget *ad_switch;
  GtkWidget *grid;
  GTlsCertificate *certificate;
  GTlsCertificateFlags tls_errors;
  EphySecurityLevel security_level;
};

G_DEFINE_TYPE (EphySecurityPopover, ephy_security_popover, GTK_TYPE_POPOVER)

static void
ephy_security_popover_set_address (EphySecurityPopover *popover,
                                   const char          *address)
{
  EphyPermissionsManager *permissions_manager;
  EphyPermission permission;
  GSettings *web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);
  SoupURI *uri;
  g_autofree gchar *origin = NULL;
  g_autofree gchar *uri_text = NULL;

  uri = soup_uri_new (address);
  uri_text = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>", uri->host);
  /* Label when clicking the lock icon on a secure page. %s is the website's hostname. */
  gtk_label_set_markup (GTK_LABEL (popover->host_label), uri_text);

  popover->address = g_strdup (address);
  popover->hostname = g_strdup (uri->host);

  soup_uri_free (uri);

  origin = ephy_uri_to_security_origin (address);
  if (!origin)
    return;

  permissions_manager = ephy_embed_shell_get_permissions_manager (ephy_embed_shell_get_default ());
  permission = ephy_permissions_manager_get_permission (permissions_manager,
                                                        EPHY_PERMISSION_TYPE_SHOW_ADS,
                                                        origin);

  switch (permission) {
    case EPHY_PERMISSION_UNDECIDED:
      gtk_switch_set_active (GTK_SWITCH (popover->ad_switch),
                             !g_settings_get_boolean (web_settings, EPHY_PREFS_WEB_ENABLE_ADBLOCK));
      break;
    case EPHY_PERMISSION_DENY:
      gtk_switch_set_active (GTK_SWITCH (popover->ad_switch), FALSE);
      break;
    case EPHY_PERMISSION_PERMIT:
      gtk_switch_set_active (GTK_SWITCH (popover->ad_switch), TRUE);
      break;
  }
}

static void
ephy_security_popover_set_certificate (EphySecurityPopover *popover,
                                       GTlsCertificate     *certificate)
{
  if (certificate)
    popover->certificate = g_object_ref (certificate);
}

static void
ephy_security_popover_set_security_level (EphySecurityPopover *popover,
                                          EphySecurityLevel    security_level)
{
  GIcon *icon;
  g_autofree gchar *address_text = NULL;
  g_autofree gchar *label_text = NULL;

  popover->security_level = security_level;

  address_text = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>", popover->hostname);
  gtk_label_set_markup (GTK_LABEL (popover->host_label), address_text);

  switch (security_level) {
    case EPHY_SECURITY_LEVEL_UNACCEPTABLE_CERTIFICATE:
      /* Label in certificate popover when site is untrusted. %s is a URL. */
      label_text = g_strdup_printf (_("This web site’s digital identification is not trusted. "
                                      "You may have connected to an attacker pretending to be %s."),
                                    address_text);
      gtk_label_set_markup (GTK_LABEL (popover->security_label), label_text);
      break;
    case EPHY_SECURITY_LEVEL_NO_SECURITY:
      /* Label in certificate popover when site uses HTTP. %s is a URL. */
      label_text = g_strdup_printf (_("This site has no security. An attacker could see any information "
                                      "you send, or control the content that you see."));
      gtk_label_set_markup (GTK_LABEL (popover->security_label), label_text);
      break;
    case EPHY_SECURITY_LEVEL_MIXED_CONTENT:
      gtk_label_set_text (GTK_LABEL (popover->security_label),
                          /* Label in certificate popover when site sends mixed content. */
                          _("This web site did not properly secure your connection."));
      break;
    case EPHY_SECURITY_LEVEL_STRONG_SECURITY:
      gtk_label_set_text (GTK_LABEL (popover->security_label),
                          /* Label in certificate popover on secure sites. */
                          _("Your connection seems to be secure."));
      break;
    case EPHY_SECURITY_LEVEL_TO_BE_DETERMINED:
    case EPHY_SECURITY_LEVEL_LOCAL_PAGE:
    default:
      g_assert_not_reached ();
  }

  icon = g_themed_icon_new_with_default_fallbacks (ephy_security_level_to_icon_name (security_level));
  gtk_image_set_from_gicon (GTK_IMAGE (popover->lock_image), icon, GTK_ICON_SIZE_BUTTON);

  g_object_unref (icon);
}

static void
certificate_button_clicked_cb (GtkButton *button,
                               gpointer   user_data)
{
  EphySecurityPopover *popover = EPHY_SECURITY_POPOVER (user_data);
  GtkWidget *dialog;

  dialog = ephy_certificate_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (popover))),
                                        popover->address,
                                        popover->certificate,
                                        popover->tls_errors,
                                        popover->security_level);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
  g_signal_connect (dialog, "response",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);

  gtk_widget_hide (GTK_WIDGET (popover));
  gtk_widget_show (dialog);
}

static void
ephy_security_popover_constructed (GObject *object)
{
  EphySecurityPopover *popover = EPHY_SECURITY_POPOVER (object);
  GtkWidget *certificate_button;

  G_OBJECT_CLASS (ephy_security_popover_parent_class)->constructed (object);

  if (!popover->certificate)
    return;

  certificate_button = gtk_button_new_with_mnemonic (_("_View Certificate…"));
  gtk_widget_set_halign (certificate_button, GTK_ALIGN_END);
  gtk_widget_set_valign (certificate_button, GTK_ALIGN_END);
  gtk_widget_set_margin_top (certificate_button, 5);
  gtk_widget_set_receives_default (certificate_button, FALSE);
  gtk_widget_show (certificate_button);
  g_signal_connect (certificate_button, "clicked",
                    G_CALLBACK (certificate_button_clicked_cb),
                    popover);

  gtk_grid_attach (GTK_GRID (popover->grid), certificate_button, 1, 2, 1, 1);
}

static void
ephy_security_popover_dispose (GObject *object)
{
  EphySecurityPopover *popover = EPHY_SECURITY_POPOVER (object);

  g_clear_object (&popover->certificate);

  G_OBJECT_CLASS (ephy_security_popover_parent_class)->dispose (object);
}

static void
ephy_security_popover_finalize (GObject *object)
{
  EphySecurityPopover *popover = EPHY_SECURITY_POPOVER (object);

  g_free (popover->address);
  g_free (popover->hostname);

  G_OBJECT_CLASS (ephy_security_popover_parent_class)->finalize (object);
}

static void
ephy_security_popover_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  EphySecurityPopover *popover = EPHY_SECURITY_POPOVER (object);

  switch (prop_id) {
    case PROP_ADDRESS:
      ephy_security_popover_set_address (popover, g_value_get_string (value));
      break;
    case PROP_CERTIFICATE:
      ephy_security_popover_set_certificate (popover, g_value_get_object (value));
      break;
    case PROP_SECURITY_LEVEL:
      ephy_security_popover_set_security_level (popover, g_value_get_enum (value));
      break;
    case PROP_TLS_ERRORS:
      popover->tls_errors = g_value_get_flags (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_security_popover_get_preferred_width (GtkWidget *widget,
                                           gint      *minimum_width,
                                           gint      *natural_width)
{
  GTK_WIDGET_CLASS (ephy_security_popover_parent_class)->get_preferred_width (widget,
                                                                              minimum_width,
                                                                              natural_width);

  if (*natural_width > 360)
    *natural_width = MAX (360, *minimum_width);
}

static void
ephy_security_popover_class_init (EphySecurityPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ephy_security_popover_constructed;
  object_class->dispose = ephy_security_popover_dispose;
  object_class->finalize = ephy_security_popover_finalize;
  object_class->set_property = ephy_security_popover_set_property;

  widget_class->get_preferred_width = ephy_security_popover_get_preferred_width;

  /**
   * EphySecurityPopover:address:
   *
   * The address of the website.
   */
  obj_properties[PROP_ADDRESS] =
    g_param_spec_string ("address",
                         "Address",
                         "The address of the website",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * EphySecurityPopover:certificate:
   *
   * The certificate of the website.
   */
  obj_properties[PROP_CERTIFICATE] =
    g_param_spec_object ("certificate",
                         "Certificate",
                         "The certificate of the website, if HTTPS",
                         G_TYPE_TLS_CERTIFICATE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * EphySecurityPopover:tls-errors:
   *
   * Indicates issues with the security of the website.
   */
  obj_properties[PROP_TLS_ERRORS] =
    g_param_spec_flags ("tls-errors",
                        "TLS Errors",
                        "Issues with the security of the website, if HTTPS",
                        G_TYPE_TLS_CERTIFICATE_FLAGS,
                        0,
                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * EphySecurityPopover:security-level:
   *
   * The state of the lock displayed in the address bar.
   */
  obj_properties[PROP_SECURITY_LEVEL] =
    g_param_spec_enum ("security-level",
                       "Security Level",
                       "Determines what type of information to display",
                       EPHY_TYPE_SECURITY_LEVEL,
                       EPHY_SECURITY_LEVEL_TO_BE_DETERMINED,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

static gboolean
on_ad_switch_state_set (GtkSwitch           *widget,
                        gboolean             state,
                        EphySecurityPopover *popover)
{
  GSettings *web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);
  EphyPermissionsManager *permissions_manager;
  EphyPermission permission = EPHY_PERMISSION_UNDECIDED;
  gboolean global_flag = !g_settings_get_boolean (web_settings, EPHY_PREFS_WEB_ENABLE_ADBLOCK);
  g_autofree gchar *origin = NULL;

  origin = ephy_uri_to_security_origin (popover->address);
  if (!origin)
    return FALSE;

  permissions_manager = ephy_embed_shell_get_permissions_manager (ephy_embed_shell_get_default ());

  if (global_flag != state)
    permission = state ? EPHY_PERMISSION_PERMIT : EPHY_PERMISSION_DENY;

  ephy_permissions_manager_set_permission (permissions_manager,
                                           EPHY_PERMISSION_TYPE_SHOW_ADS,
                                           origin,
                                           permission);

  return FALSE;
}

static void
ephy_security_popover_init (EphySecurityPopover *popover)
{
  GtkWidget *adblocker;
  GtkWidget *adblock_desc;
  GtkWidget *hbox;
  GtkWidget *box;
  g_autofree char *label = g_strdup_printf ("<b>%s</b>", _("Permissions"));

  popover->grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (popover->grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (popover->grid), 6);
  g_object_set (popover->grid, "margin", 10, NULL);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (box, TRUE);

  popover->lock_image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (box), popover->lock_image, FALSE, FALSE, 0);

  popover->host_label = gtk_label_new (NULL);
  gtk_label_set_line_wrap (GTK_LABEL (popover->host_label), TRUE);
  gtk_label_set_line_wrap_mode (GTK_LABEL (popover->host_label), PANGO_WRAP_WORD_CHAR);
  gtk_label_set_xalign (GTK_LABEL (popover->host_label), 0.0);
  gtk_box_pack_start (GTK_BOX (box), popover->host_label, FALSE, FALSE, 0);

  popover->security_label = gtk_label_new (NULL);
  gtk_label_set_line_wrap (GTK_LABEL (popover->security_label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (popover->security_label), 0.0);

  gtk_grid_attach (GTK_GRID (popover->grid), box, 0, 0, 2, 1);
  gtk_grid_attach (GTK_GRID (popover->grid), popover->security_label, 0, 1, 2, 1);

  gtk_grid_attach (GTK_GRID (popover->grid), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), 0, 3, 2, 1);

  /* Permissions */
  adblocker = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (adblocker), label);
  gtk_label_set_xalign (GTK_LABEL (adblocker), 0.0);
  gtk_grid_attach (GTK_GRID (popover->grid), adblocker, 0, 4, 2, 1);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_grid_attach (GTK_GRID (popover->grid), hbox, 0, 5, 2, 1);

  adblock_desc = gtk_label_new (_("Allow advertisements"));
  gtk_label_set_xalign (GTK_LABEL (adblock_desc), 0.0);
  gtk_box_pack_start (GTK_BOX (hbox), adblock_desc, TRUE, TRUE, 6);

  popover->ad_switch = gtk_switch_new ();
  gtk_box_pack_start (GTK_BOX (hbox), popover->ad_switch, FALSE, FALSE, 6);
  g_signal_connect (popover->ad_switch, "state-set", G_CALLBACK (on_ad_switch_state_set), popover);

  gtk_container_add (GTK_CONTAINER (popover), popover->grid);
  gtk_widget_show_all (popover->grid);
}

GtkWidget *ephy_security_popover_new (GtkWidget           *relative_to,
                                      const char          *address,
                                      GTlsCertificate     *certificate,
                                      GTlsCertificateFlags tls_errors,
                                      EphySecurityLevel    security_level)
{
  g_assert (address != NULL);

  return GTK_WIDGET (g_object_new (EPHY_TYPE_SECURITY_POPOVER,
                                   "address", address,
                                   "certificate", certificate,
                                   "relative-to", relative_to,
                                   "security-level", security_level,
                                   "tls-errors", tls_errors,
                                   NULL));
}
