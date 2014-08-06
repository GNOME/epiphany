/*
 *  Copyright © 2012 Igalia S.L.
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

#include "config.h"
#include "ephy-certificate-dialog.h"

#define GCR_API_SUBJECT_TO_CHANGE
#include <gcr/gcr.h>
#include <glib/gi18n.h>
#include <libsoup/soup.h>

/**
 * SECTION:ephy-certificate-dialog
 * @short_description: A dialog to show SSL certificate information
 *
 * #EphyCertificateDialog shows information about SSL certificates.
 */

enum
{
  PROP_0,
  PROP_ADDRESS,
  PROP_CERTIFICATE,
  PROP_TLS_ERRORS
};

struct _EphyCertificateDialogPrivate
{
  GtkWidget *icon;
  GtkWidget *title;
  GtkWidget *text;
};

G_DEFINE_TYPE (EphyCertificateDialog, ephy_certificate_dialog, GTK_TYPE_DIALOG)

static void
ephy_certificate_dialog_set_address (EphyCertificateDialog *dialog,
                                     const char *address)
{
  SoupURI *uri;

  uri = soup_uri_new (address);
  gtk_window_set_title (GTK_WINDOW (dialog), uri->host);
  soup_uri_free (uri);
}

static void
ephy_certificate_dialog_set_certificate (EphyCertificateDialog *dialog,
                                         GTlsCertificate *certificate)
{
  GcrCertificate *simple_certificate;
  GByteArray *certificate_data;
  GtkWidget *certificate_widget;
  GtkWidget *content_area;

  g_object_get (certificate, "certificate", &certificate_data, NULL);
  simple_certificate = gcr_simple_certificate_new ((const guchar *)certificate_data->data,
						   certificate_data->len);
  g_byte_array_unref (certificate_data);

  certificate_widget = GTK_WIDGET (gcr_certificate_widget_new (simple_certificate));
  g_object_unref (simple_certificate);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_box_pack_start (GTK_BOX (content_area), certificate_widget, TRUE, TRUE, 0);
  gtk_widget_show (certificate_widget);
}

static char *
get_error_messages_from_tls_errors (GTlsCertificateFlags tls_errors)
{
  GPtrArray *errors = g_ptr_array_new ();
  char *retval;

  if (tls_errors & G_TLS_CERTIFICATE_BAD_IDENTITY)
    g_ptr_array_add (errors, _("The certificate does not match this website"));

  if (tls_errors & G_TLS_CERTIFICATE_EXPIRED)
    g_ptr_array_add (errors, _("The certificate has expired"));

  if (tls_errors & G_TLS_CERTIFICATE_UNKNOWN_CA)
    g_ptr_array_add (errors, _("The signing certificate authority is not known"));

  if (tls_errors & G_TLS_CERTIFICATE_GENERIC_ERROR)
    g_ptr_array_add (errors, _("The certificate contains errors"));

  if (tls_errors & G_TLS_CERTIFICATE_REVOKED)
    g_ptr_array_add (errors, _("The certificate has been revoked"));

  if (tls_errors & G_TLS_CERTIFICATE_INSECURE)
    g_ptr_array_add (errors, _("The certificate is signed using a weak signature algorithm"));

  if (tls_errors & G_TLS_CERTIFICATE_NOT_ACTIVATED)
    g_ptr_array_add (errors, _("The certificate activation time is still in the future"));

  if (errors->len == 1)
    retval = g_strdup (g_ptr_array_index (errors, 0));
  else {
    GString *message = g_string_new (NULL);
    guint i;

    for (i = 0; i < errors->len; i++) {
      g_string_append_printf (message, "• %s",
			      (char *)g_ptr_array_index (errors, i));
      if (i < errors->len - 1)
	g_string_append_c (message, '\n');
    }

    retval = g_string_free (message, FALSE);
  }

  g_ptr_array_free (errors, TRUE);

  return retval;
}

static void
ephy_certificate_dialog_set_tls_errors (EphyCertificateDialog *dialog,
                                        GTlsCertificateFlags tls_errors)
{
  EphyCertificateDialogPrivate *priv = dialog->priv;
  GIcon *icon;
  char *markup;

  icon = tls_errors == 0 ?
    g_themed_icon_new_with_default_fallbacks ("channel-secure-symbolic") :
    g_themed_icon_new_with_default_fallbacks ("channel-insecure-symbolic");
  gtk_image_set_from_gicon (GTK_IMAGE (priv->icon), icon, GTK_ICON_SIZE_DIALOG);
  g_object_unref (icon);

  markup = g_strdup_printf ("<span weight=\"bold\" size=\"large\">%s</span>",
			    tls_errors == 0 ?
			    _("The identity of this website has been verified.") :
			    _("The identity of this website has not been verified."));
  gtk_label_set_markup (GTK_LABEL (priv->title), markup);
  g_free (markup);

  if (tls_errors) {
    char *text = get_error_messages_from_tls_errors (tls_errors);

    gtk_label_set_text (GTK_LABEL (priv->text), text);
    g_free (text);

    gtk_widget_show (priv->text);
  }
}

static void
ephy_certificate_dialog_set_property (GObject *object,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
  EphyCertificateDialog *dialog = EPHY_CERTIFICATE_DIALOG (object);

  switch (prop_id) {
  case PROP_ADDRESS:
    ephy_certificate_dialog_set_address (dialog, g_value_get_string (value));
    break;
  case PROP_CERTIFICATE:
    ephy_certificate_dialog_set_certificate (dialog, g_value_get_object (value));
    break;
  case PROP_TLS_ERRORS:
    ephy_certificate_dialog_set_tls_errors (dialog, g_value_get_flags (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_certificate_dialog_class_init (EphyCertificateDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_certificate_dialog_set_property;

  /**
   * EphyCertificateDialog:address:
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
							G_PARAM_STATIC_NAME |
							G_PARAM_STATIC_NICK |
							G_PARAM_STATIC_BLURB));

  /**
   * EphyCertificateDialog:certificate:
   *
   * The certificate of the website.
   */
  g_object_class_install_property (object_class,
				   PROP_CERTIFICATE,
				   g_param_spec_object ("certificate",
							"Certificate",
							"The certificate of the website",
							G_TYPE_TLS_CERTIFICATE,
							G_PARAM_WRITABLE |
							G_PARAM_CONSTRUCT_ONLY |
							G_PARAM_STATIC_NAME |
							G_PARAM_STATIC_NICK |
							G_PARAM_STATIC_BLURB));

  /**
   * EphyCertificateDialog:tls-errors:
   *
   * The verification errors on the TLS certificate.
   */
  g_object_class_install_property (object_class,
				   PROP_TLS_ERRORS,
				   g_param_spec_flags ("tls-errors",
						       "TLS Errors",
						       "The verification errors on the TLS certificate",
						       G_TYPE_TLS_CERTIFICATE_FLAGS,
						       0,
						       G_PARAM_WRITABLE |
						       G_PARAM_CONSTRUCT_ONLY |
						       G_PARAM_STATIC_NAME |
						       G_PARAM_STATIC_NICK |
						       G_PARAM_STATIC_BLURB));

  g_type_class_add_private (object_class, sizeof (EphyCertificateDialogPrivate));
}

static void
ephy_certificate_dialog_init (EphyCertificateDialog *dialog)
{
  GtkWidget *grid;
  GtkWidget *content_area;
  EphyCertificateDialogPrivate *priv;

  dialog->priv = G_TYPE_INSTANCE_GET_PRIVATE (dialog,
					      EPHY_TYPE_CERTIFICATE_DIALOG,
					      EphyCertificateDialogPrivate);
  priv = dialog->priv;

  gtk_window_set_default_size (GTK_WINDOW (dialog), -1, 500);

  gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), TRUE);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);

  priv->icon = gtk_image_new ();
  gtk_widget_set_halign (priv->icon, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (priv->icon, GTK_ALIGN_START);
  gtk_grid_attach (GTK_GRID (grid), priv->icon,
                   0, 0, 1, 2);
  gtk_widget_show (priv->icon);

  priv->title = gtk_label_new (NULL);
  gtk_label_set_use_markup (GTK_LABEL (priv->title), TRUE);
  gtk_label_set_line_wrap  (GTK_LABEL (priv->title), TRUE);
  gtk_label_set_selectable (GTK_LABEL (priv->title), TRUE);
  gtk_widget_set_halign (priv->title, GTK_ALIGN_START);
  gtk_widget_set_valign (priv->title, GTK_ALIGN_CENTER);
  gtk_misc_set_alignment (GTK_MISC (priv->title), 0.0, 0.5);
  gtk_grid_attach_next_to (GTK_GRID (grid), priv->title,
                           priv->icon, GTK_POS_RIGHT,
                           1, 1);
  gtk_widget_show (priv->title);

  priv->text = gtk_label_new (NULL);
  gtk_label_set_line_wrap  (GTK_LABEL (priv->text), TRUE);
  gtk_label_set_selectable (GTK_LABEL (priv->text), TRUE);
  gtk_widget_set_halign (priv->text, GTK_ALIGN_START);
  gtk_widget_set_valign (priv->text, GTK_ALIGN_START);
  gtk_misc_set_alignment (GTK_MISC (priv->text), 0.0, 0.0);
  gtk_grid_attach_next_to (GTK_GRID (grid), priv->text,
                           priv->title, GTK_POS_BOTTOM,
                           1, 1);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_box_set_spacing (GTK_BOX (content_area), 14);
  gtk_box_pack_start (GTK_BOX (content_area), grid, FALSE, FALSE, 0);
  gtk_widget_show (grid);
}

GtkWidget *
ephy_certificate_dialog_new (GtkWindow *parent,
                             const char *address,
                             GTlsCertificate *certificate,
                             GTlsCertificateFlags tls_errors)
{
  GtkWidget *dialog;

  g_return_val_if_fail (address != NULL, NULL);
  g_return_val_if_fail (G_IS_TLS_CERTIFICATE (certificate), NULL);

  dialog = GTK_WIDGET (g_object_new (EPHY_TYPE_CERTIFICATE_DIALOG,
				     "address", address,
				     "certificate", certificate,
				     "tls-errors", tls_errors,
                                     "modal", TRUE,
                                     "use-header-bar", TRUE,
				     NULL));
  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  return dialog;
}

