/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2014, 2015 Igalia S.L.
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-security-popover.h"

#include <glib/gi18n.h>
#include <libsoup/soup.h>

#include "ephy-certificate-dialog.h"
#include "ephy-lib-type-builtins.h"

/**
 * SECTION:ephy-security-popover
 * @short_description: A popover to show basic TLS connection information
 *
 * #EphySecurityPopover shows basic information about a TLS connection
 * and allows opening #EphyCertificateDialog for more detailed information. It
 * can also be used to show that a connection does not use TLS at all.
 */

enum
{
  PROP_0,
  PROP_ADDRESS,
  PROP_CERTIFICATE,
  PROP_SECURITY_LEVEL,
  PROP_TLS_ERRORS,
};

struct _EphySecurityPopoverPrivate
{
  char *address;
  char *hostname;
  GtkWidget *lock_image;
  GtkWidget *host_label;
  GtkWidget *security_label;
  GtkWidget *grid;
  GTlsCertificate *certificate;
  GTlsCertificateFlags tls_errors;
  EphySecurityLevel security_level;
};

G_DEFINE_TYPE_WITH_PRIVATE (EphySecurityPopover, ephy_security_popover, GTK_TYPE_POPOVER)

static void
ephy_security_popover_set_address (EphySecurityPopover *popover,
                                   const char *address)
{
  EphySecurityPopoverPrivate *priv = popover->priv;
  SoupURI *uri;
  char *label_text;
  char *uri_text;

  uri = soup_uri_new (address);
  uri_text = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>.", uri->host);
  /* Label when clicking the lock icon on a secure page. %s is the website's hostname. */
  label_text = g_strdup_printf (_("You are connected to %s"), uri_text);
  gtk_label_set_markup (GTK_LABEL (priv->host_label), label_text);

  priv->address = g_strdup (address);
  priv->hostname = g_strdup (uri->host);

  soup_uri_free (uri);
  g_free (label_text);
  g_free (uri_text);
}

static void
ephy_security_popover_set_certificate (EphySecurityPopover *popover,
                                       GTlsCertificate *certificate)
{
  EphySecurityPopoverPrivate *priv = popover->priv;

  if (certificate)
    priv->certificate = g_object_ref (certificate);
}

static void
ephy_security_popover_set_security_level (EphySecurityPopover *popover,
                                          EphySecurityLevel security_level)
{
  EphySecurityPopoverPrivate *priv = popover->priv;
  GIcon *icon;
  char *address_text;
  char *label_text = NULL;

  priv->security_level = security_level;

  address_text = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>", priv->hostname);

  switch (security_level) {
  case EPHY_SECURITY_LEVEL_UNACCEPTABLE_CERTIFICATE:
    /* Label in certificate popover when site is untrusted. %s is a URL. */
    label_text = g_strdup_printf (_("This web site’s digital identification is not trusted. "
                                    "You may have connected to an attacker pretending to be %s."),
                                  address_text);
    gtk_label_set_markup (GTK_LABEL (priv->security_label), label_text);
    gtk_widget_hide (priv->host_label);
    break;
  case EPHY_SECURITY_LEVEL_NO_SECURITY:
    /* Label in certificate popover when site uses HTTP. %s is a URL. */
    label_text = g_strdup_printf (_("%s has no security. An attacker could see any information "
                                    "you send, or control the content that you see."),
                                  address_text);
    gtk_label_set_markup (GTK_LABEL (priv->security_label), label_text);
    gtk_widget_hide (priv->host_label);
    break;
  case EPHY_SECURITY_LEVEL_MIXED_CONTENT:
    gtk_label_set_text (GTK_LABEL (priv->security_label),
                        /* Label in certificate popover when site sends mixed content. */
                        _("This web site did not properly secure your connection."));
    gtk_widget_show (priv->host_label);
    break;
  case EPHY_SECURITY_LEVEL_STRONG_SECURITY:
    gtk_label_set_text (GTK_LABEL (priv->security_label),
                        /* Label in certificate popover on secure sites. */
                        _("Your connection seems to be secure."));
    gtk_widget_show (priv->host_label);
    break;
  default:
    g_assert_not_reached ();
  }

  icon = g_themed_icon_new_with_default_fallbacks (ephy_security_level_to_icon_name (security_level));
  gtk_image_set_from_gicon (GTK_IMAGE (priv->lock_image), icon, GTK_ICON_SIZE_DIALOG);

  g_free (address_text);
  g_free (label_text);
  g_object_unref (icon);
}

static void
certificate_button_clicked_cb (GtkButton *button,
                               gpointer user_data)
{
  EphySecurityPopover *popover = EPHY_SECURITY_POPOVER (user_data);
  EphySecurityPopoverPrivate *priv = popover->priv;
  GtkWidget *dialog;

  dialog = ephy_certificate_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (popover))),
                                        priv->address,
                                        priv->certificate,
                                        priv->tls_errors,
                                        priv->security_level);
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
  EphySecurityPopoverPrivate *priv = popover->priv;
  GtkWidget *certificate_button;

  G_OBJECT_CLASS (ephy_security_popover_parent_class)->constructed (object);

  if (!priv->certificate)
    return;

  certificate_button = gtk_button_new_with_mnemonic (_("_View Certificate…"));
  gtk_widget_set_halign (certificate_button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (certificate_button, GTK_ALIGN_END);
  gtk_widget_set_margin_top (certificate_button, 5);
  gtk_widget_set_receives_default (certificate_button, FALSE);
  gtk_widget_show (certificate_button);
  g_signal_connect (certificate_button, "clicked",
                    G_CALLBACK (certificate_button_clicked_cb),
                    popover);

  gtk_grid_attach (GTK_GRID (priv->grid), certificate_button, 2, 1, 1, 1);
}

static void
ephy_security_popover_dispose (GObject *object)
{
  EphySecurityPopover *popover = EPHY_SECURITY_POPOVER (object);
  EphySecurityPopoverPrivate *priv = popover->priv;

  g_clear_object (&priv->certificate);

  G_OBJECT_CLASS (ephy_security_popover_parent_class)->dispose (object);
}

static void
ephy_security_popover_finalize (GObject *object)
{
  EphySecurityPopover *popover = EPHY_SECURITY_POPOVER (object);
  EphySecurityPopoverPrivate *priv = popover->priv;

  g_free (priv->address);
  g_free (priv->hostname);

  G_OBJECT_CLASS (ephy_security_popover_parent_class)->finalize (object);
}

static void
ephy_security_popover_set_property (GObject *object,
                                    guint prop_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
  EphySecurityPopover *popover = EPHY_SECURITY_POPOVER (object);
  EphySecurityPopoverPrivate *priv = popover->priv;

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
    priv->tls_errors = g_value_get_flags (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_security_popover_get_preferred_width (GtkWidget *widget,
                                           gint *minimum_width,
                                           gint *natural_width)
{
  GTK_WIDGET_CLASS (ephy_security_popover_parent_class)->get_preferred_width (widget,
                                                                                 minimum_width,
                                                                                 natural_width);

  if (*natural_width > 600)
    *natural_width = MAX(600, *minimum_width);
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
  g_object_class_install_property (object_class,
                                   PROP_ADDRESS,
                                   g_param_spec_string ("address",
                                                        "Address",
                                                        "The address of the website",
                                                        NULL,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * EphySecurityPopover:certificate:
   *
   * The certificate of the website.
   */
  g_object_class_install_property (object_class,
                                   PROP_CERTIFICATE,
                                   g_param_spec_object ("certificate",
                                                        "Certificate",
                                                        "The certificate of the website, if HTTPS",
                                                        G_TYPE_TLS_CERTIFICATE,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * EphySecurityPopover:tls-errors:
   *
   * Indicates issues with the security of the website.
   */
  g_object_class_install_property (object_class,
                                   PROP_TLS_ERRORS,
                                   g_param_spec_flags ("tls-errors",
                                                       "TLS Errors",
                                                       "Issues with the security of the website, if HTTPS",
                                                       G_TYPE_TLS_CERTIFICATE_FLAGS,
                                                       0,
                                                       G_PARAM_WRITABLE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_STRINGS));

  /**
   * EphySecurityPopover:security-level:
   *
   * The state of the lock displayed in the address bar.
   */
  g_object_class_install_property (object_class,
                                   PROP_SECURITY_LEVEL,
                                   g_param_spec_enum ("security-level",
                                                      "Security Level",
                                                      "Determines what type of information to display",
                                                      EPHY_TYPE_SECURITY_LEVEL,
                                                      EPHY_SECURITY_LEVEL_TO_BE_DETERMINED,
                                                      G_PARAM_WRITABLE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_STRINGS));
}

static void
ephy_security_popover_init (EphySecurityPopover *popover)
{
  EphySecurityPopoverPrivate *priv;

  popover->priv = ephy_security_popover_get_instance_private (popover);
  priv = popover->priv;

  priv->grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (priv->grid), 15);
  g_object_set (priv->grid, "margin", 10, NULL);

  priv->lock_image = gtk_image_new ();

  priv->host_label = gtk_label_new (NULL);
  gtk_widget_set_halign (priv->host_label, GTK_ALIGN_START);

  priv->security_label = gtk_label_new (NULL);
  gtk_label_set_line_wrap (GTK_LABEL (priv->security_label), TRUE);
  /* We must use deprecated GtkMisc, not halign, as GTK_ALIGN_START fails for labels with line wrap. */
  gtk_misc_set_alignment (GTK_MISC (priv->security_label), 0.0, 0.5);

  gtk_grid_attach (GTK_GRID (priv->grid), priv->lock_image, 0, 0, 1, 2);
  gtk_grid_attach (GTK_GRID (priv->grid), priv->host_label, 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (priv->grid), priv->security_label, 1, 1, 1, 1);

  gtk_container_add (GTK_CONTAINER (popover), priv->grid);
  gtk_widget_show_all (priv->grid);
}

GtkWidget *ephy_security_popover_new (GtkWidget *relative_to,
                                      const char *address,
                                      GTlsCertificate *certificate,
                                      GTlsCertificateFlags tls_errors,
                                      EphySecurityLevel security_level)
{
  g_return_val_if_fail (address != NULL, NULL);

  return GTK_WIDGET (g_object_new (EPHY_TYPE_SECURITY_POPOVER,
                                   "address", address,
                                   "certificate", certificate,
                                   "relative-to", relative_to,
                                   "security-level", security_level,
                                   "tls-errors", tls_errors,
                                   NULL));
}
