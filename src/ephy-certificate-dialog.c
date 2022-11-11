/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
 *  Copyright © 2021 Collabora Ltd.
 *  Copyright © Corentin Noël <corentin.noel@collabora.com>
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
#include "ephy-certificate-dialog.h"

#include "ephy-lib-type-builtins.h"

#include <adwaita.h>
#define GCK_API_SUBJECT_TO_CHANGE
#define GCR_API_SUBJECT_TO_CHANGE
#include <gcr-4/gcr/gcr.h>
#include <glib/gi18n.h>

/**
 * SECTION:ephy-certificate-dialog
 * @short_description: A dialog to show SSL certificate information
 *
 * #EphyCertificateDialog shows information about SSL certificates.
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

struct _EphyCertificateDialog {
  GtkDialog parent_object;

  GtkWidget *icon;
  GtkWidget *title;
  GtkWidget *text;
  GtkWidget *box;

  GTlsCertificateFlags tls_errors;
  EphySecurityLevel security_level;
};

G_DEFINE_TYPE (EphyCertificateDialog, ephy_certificate_dialog, GTK_TYPE_DIALOG)

static char *
bytes_to_display (GBytes *bytes)
{
  const char *hexc = "0123456789ABCDEF";
  GString *result;
  const char *input;
  gsize read_bytes;
  gsize remaining_bytes;
  guchar j;

  g_return_val_if_fail (bytes != NULL, NULL);

  input = g_bytes_get_data (bytes, &remaining_bytes);
  result = g_string_sized_new (remaining_bytes * 2 + 1);

  for (read_bytes = 0; read_bytes < remaining_bytes; read_bytes++) {
    if (read_bytes && (read_bytes % 1) == 0)
      g_string_append_c (result, ' ');

    j = *(input) >> 4 & 0xf;
    g_string_append_c (result, hexc[j]);

    j = *(input++) & 0xf;
    g_string_append_c (result, hexc[j]);
  }

  return g_string_free (result, FALSE);
}

static GtkWidget *
create_section_row (GcrCertificateField *field,
                    GtkSizeGroup        *size_group)
{
  GtkWidget *row, *box, *label, *value;
  GValue val = G_VALUE_INIT;
  GType value_type;

  g_return_val_if_fail (GCR_IS_CERTIFICATE_FIELD (field), NULL);

  row = gtk_list_box_row_new ();
  gtk_widget_set_focusable (row, FALSE);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_start (box, 12);
  gtk_widget_set_margin_end (box, 12);
  gtk_widget_set_margin_top (box, 12);
  gtk_widget_set_margin_bottom (box, 12);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);

  label = gtk_label_new (gcr_certificate_field_get_label (field));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.0);
  gtk_box_append (GTK_BOX (box), label);

  value = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (value), 1.0);
  gtk_label_set_yalign (GTK_LABEL (value), 0.0);
  gtk_label_set_selectable (GTK_LABEL (value), TRUE);
  gtk_label_set_wrap (GTK_LABEL (value), TRUE);
  gtk_label_set_wrap_mode (GTK_LABEL (value), PANGO_WRAP_WORD_CHAR);
  gtk_widget_set_hexpand (value, TRUE);
  gtk_box_append (GTK_BOX (box), value);

  value_type = gcr_certificate_field_get_value_type (field);
  g_value_init (&val, value_type);
  gcr_certificate_field_get_value (field, &val);

  if (g_type_is_a (value_type, G_TYPE_STRING)) {
    gtk_label_set_label (GTK_LABEL (value), g_value_get_string (&val));
  } else if (g_type_is_a (value_type, G_TYPE_STRV)) {
    g_autofree char *lbl = g_strjoinv ("\n", (GStrv)g_value_get_boxed (&val));

    gtk_label_set_label (GTK_LABEL (value), lbl);
    gtk_label_set_justify (GTK_LABEL (value), GTK_JUSTIFY_RIGHT);
  } else if (g_type_is_a (value_type, G_TYPE_BYTES)) {
    GBytes *bytes = g_value_get_boxed (&val);
    g_autofree char *lbl = bytes_to_display (bytes);

    gtk_label_set_label (GTK_LABEL (value), lbl);
    gtk_widget_add_css_class (value, "monospace");
  }

  g_value_unset (&val);

  gtk_size_group_add_widget (size_group, label);

  return row;
}

static GtkWidget *
create_section (GcrCertificateSection *section)
{
  GtkWidget *group, *listbox;
  GtkSizeGroup *size_group;

  group = adw_preferences_group_new ();
  g_object_bind_property (section, "label", group, "title", G_BINDING_SYNC_CREATE);

  listbox = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (listbox), GTK_SELECTION_NONE);
  gtk_widget_add_css_class (listbox, "boxed-list");
  adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), listbox);

  size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  gtk_list_box_bind_model (GTK_LIST_BOX (listbox),
                           gcr_certificate_section_get_fields (section),
                           (GtkListBoxCreateWidgetFunc)create_section_row,
                           size_group, g_object_unref);

  return group;
}

static void
reveal_clicked_cb (GtkWidget *button,
                   GtkWidget *secondary_info)
{
  gtk_widget_hide (button);
  gtk_widget_show (secondary_info);
}

static GtkWidget *
create_page (GcrCertificate *certificate)
{
  GtkWidget *box, *primary_info, *secondary_info, *reveal_button;
  GList *elements, *l;
  GtkSizeGroup *size_group;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);

  primary_info = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_box_append (GTK_BOX (box), primary_info);

  reveal_button = gtk_button_new_from_icon_name ("view-more-horizontal-symbolic");
  gtk_widget_set_halign (reveal_button, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (box), reveal_button);

  secondary_info = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_hide (secondary_info);
  gtk_box_append (GTK_BOX (box), secondary_info);

  size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  gtk_size_group_add_widget (size_group, primary_info);
  gtk_size_group_add_widget (size_group, secondary_info);

  elements = gcr_certificate_get_interface_elements (certificate);

  for (l = elements; l != NULL; l = l->next) {
    GcrCertificateSection *section = l->data;
    GtkWidget *widget = create_section (section);

    if (gcr_certificate_section_get_flags (section) & GCR_CERTIFICATE_SECTION_IMPORTANT)
      gtk_box_append (GTK_BOX (primary_info), widget);
    else
      gtk_box_append (GTK_BOX (secondary_info), widget);
  }

  g_list_free_full (elements, (GDestroyNotify)g_object_unref);

  g_signal_connect (reveal_button, "clicked",
                    G_CALLBACK (reveal_clicked_cb), secondary_info);

  return box;
}

static void
ephy_certificate_dialog_set_address (EphyCertificateDialog *dialog,
                                     const char            *address)
{
  g_autoptr (GUri) uri = NULL;

  uri = g_uri_parse (address, G_URI_FLAGS_NONE, NULL);
  gtk_window_set_title (GTK_WINDOW (dialog), g_uri_get_host (uri));
}

static void
ephy_certificate_dialog_set_certificate (EphyCertificateDialog *dialog,
                                         GTlsCertificate       *certificate)
{
  GcrCertificate *simple_certificate;
  GByteArray *certificate_data;
  GtkWidget *page;

  g_object_get (certificate, "certificate", &certificate_data, NULL);
  simple_certificate = gcr_simple_certificate_new ((const guchar *)certificate_data->data,
                                                   certificate_data->len);
  g_byte_array_unref (certificate_data);

  page = create_page (simple_certificate);
  gtk_widget_set_vexpand (page, TRUE);
  g_object_unref (simple_certificate);

  gtk_box_append (GTK_BOX (dialog->box), page);
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
ephy_certificate_dialog_constructed (GObject *object)
{
  EphyCertificateDialog *dialog = EPHY_CERTIFICATE_DIALOG (object);
  GIcon *icon;
  const char *icon_name;
  char *markup;

  G_OBJECT_CLASS (ephy_certificate_dialog_parent_class)->constructed (object);

  icon_name = ephy_security_level_to_icon_name (dialog->security_level);
  if (icon_name) {
    icon = g_themed_icon_new_with_default_fallbacks (icon_name);
    gtk_image_set_from_gicon (GTK_IMAGE (dialog->icon), icon);
    g_object_unref (icon);
  }

  markup = g_strdup_printf ("<span weight=\"bold\" size=\"large\">%s</span>",
                            dialog->tls_errors == 0 ?
                            _("The identity of this website has been verified.") :
                            _("The identity of this website has not been verified."));
  gtk_label_set_markup (GTK_LABEL (dialog->title), markup);
  g_free (markup);

  if (dialog->tls_errors) {
    char *text = get_error_messages_from_tls_errors (dialog->tls_errors);
    gtk_label_set_text (GTK_LABEL (dialog->text), text);
    g_free (text);
  } else {
    switch (dialog->security_level) {
      case EPHY_SECURITY_LEVEL_STRONG_SECURITY:
        /* Message on certificte dialog ertificate dialog */
        gtk_label_set_text (GTK_LABEL (dialog->text), _("No problems have been detected with your connection."));
        break;
      case EPHY_SECURITY_LEVEL_MIXED_CONTENT:
        gtk_label_set_text (GTK_LABEL (dialog->text), _("This certificate is valid. However, "
                                                        "resources on this page were sent insecurely."));
        break;
      case EPHY_SECURITY_LEVEL_TO_BE_DETERMINED:
      case EPHY_SECURITY_LEVEL_NO_SECURITY:
      case EPHY_SECURITY_LEVEL_UNACCEPTABLE_CERTIFICATE:
      case EPHY_SECURITY_LEVEL_LOCAL_PAGE:
      default:
        g_assert_not_reached ();
    }
  }
}

static void
ephy_certificate_dialog_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  EphyCertificateDialog *dialog = EPHY_CERTIFICATE_DIALOG (object);

  switch (prop_id) {
    case PROP_ADDRESS:
      ephy_certificate_dialog_set_address (dialog, g_value_get_string (value));
      break;
    case PROP_CERTIFICATE:
      ephy_certificate_dialog_set_certificate (dialog, g_value_get_object (value));
      break;
    case PROP_SECURITY_LEVEL:
      dialog->security_level = g_value_get_enum (value);
      break;
    case PROP_TLS_ERRORS:
      dialog->tls_errors = g_value_get_flags (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_certificate_dialog_class_init (EphyCertificateDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ephy_certificate_dialog_constructed;
  object_class->set_property = ephy_certificate_dialog_set_property;

  /**
   * EphyCertificateDialog:address:
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
   * EphyCertificateDialog:certificate:
   *
   * The certificate of the website.
   */
  obj_properties[PROP_CERTIFICATE] =
    g_param_spec_object ("certificate",
                         "Certificate",
                         "The certificate of the website",
                         G_TYPE_TLS_CERTIFICATE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * EphySecurityLevel:security-level:
   *
   * Indicates whether something is wrong with the connection.
   */
  obj_properties[PROP_SECURITY_LEVEL] =
    g_param_spec_enum ("security-level",
                       "Security Level",
                       "Indicates whether something is wrong with the connection",
                       EPHY_TYPE_SECURITY_LEVEL,
                       EPHY_SECURITY_LEVEL_TO_BE_DETERMINED,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * EphyCertificateDialog:tls-errors:
   *
   * The verification errors on the TLS certificate.
   */
  obj_properties[PROP_TLS_ERRORS] =
    g_param_spec_flags ("tls-errors",
                        "TLS Errors",
                        "The verification errors on the TLS certificate",
                        G_TYPE_TLS_CERTIFICATE_FLAGS,
                        0,
                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

static void
ephy_certificate_dialog_init (EphyCertificateDialog *dialog)
{
  GtkWidget *grid;
  GtkWidget *scrolled_window;
  GtkWidget *content_area;
  GtkWidget *clamp;

  gtk_window_set_default_size (GTK_WINDOW (dialog), -1, 500);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);

  dialog->icon = gtk_image_new ();
  gtk_image_set_pixel_size (GTK_IMAGE (dialog->icon), 48);
  gtk_grid_attach (GTK_GRID (grid), dialog->icon,
                   0, 0, 1, 2);

  dialog->title = gtk_label_new (NULL);
  gtk_label_set_use_markup (GTK_LABEL (dialog->title), TRUE);
  gtk_label_set_wrap (GTK_LABEL (dialog->title), TRUE);
  gtk_label_set_selectable (GTK_LABEL (dialog->title), TRUE);
  gtk_label_set_xalign (GTK_LABEL (dialog->title), 0.0);
  gtk_grid_attach_next_to (GTK_GRID (grid), dialog->title,
                           dialog->icon, GTK_POS_RIGHT,
                           1, 1);

  dialog->text = gtk_label_new (NULL);
  gtk_label_set_wrap (GTK_LABEL (dialog->text), TRUE);
  gtk_label_set_selectable (GTK_LABEL (dialog->text), TRUE);
  gtk_label_set_xalign (GTK_LABEL (dialog->text), 0.0);
  gtk_label_set_yalign (GTK_LABEL (dialog->text), 0.0);
  gtk_grid_attach_next_to (GTK_GRID (grid), dialog->text,
                           dialog->title, GTK_POS_BOTTOM,
                           1, 1);

  dialog->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_top (dialog->box, 12);
  gtk_widget_set_margin_bottom (dialog->box, 12);
  gtk_widget_set_margin_start (dialog->box, 12);
  gtk_widget_set_margin_end (dialog->box, 12);
  gtk_box_append (GTK_BOX (dialog->box), grid);

  clamp = adw_clamp_new ();
  adw_clamp_set_child (ADW_CLAMP (clamp), dialog->box);

  scrolled_window = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), clamp);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_box_append (GTK_BOX (content_area), scrolled_window);
}

GtkWidget *
ephy_certificate_dialog_new (GtkWindow            *parent,
                             const char           *address,
                             GTlsCertificate      *certificate,
                             GTlsCertificateFlags  tls_errors,
                             EphySecurityLevel     security_level)
{
  GtkWidget *dialog;

  g_assert (address != NULL);
  g_assert (G_IS_TLS_CERTIFICATE (certificate));

  dialog = GTK_WIDGET (g_object_new (EPHY_TYPE_CERTIFICATE_DIALOG,
                                     "address", address,
                                     "certificate", certificate,
                                     "security-level", security_level,
                                     "tls-errors", tls_errors,
                                     "modal", TRUE,
                                     "use-header-bar", TRUE,
                                     "width-request", 500,
                                     NULL));
  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  return dialog;
}
