/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2021-2023 Jan-Michael Brummer <jan.brummer@tabos.org>
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#include "autofill-view.h"
#include "ephy-autofill-field.h"
#include "ephy-autofill-storage.h"
#include "ephy-shell.h"
#include "ephy-uri-helpers.h"

struct _EphyAutoFillView {
  AdwNavigationPage parent_instance;

  GtkWidget *listbox;
  GtkWidget *confirmation_dialog;
  GtkWidget *first_name;
  GtkWidget *last_name;
  GtkWidget *full_name;
  GtkWidget *user_name;
  GtkWidget *email;
  GtkWidget *phone;
  GtkWidget *street;
  GtkWidget *organization;
  GtkWidget *postal_code;
  GtkWidget *state;
  GtkWidget *city;
  GtkWidget *country;
  GtkWidget *card_type;
  GtkWidget *card_owner;
  GtkWidget *card_number;

  GActionGroup *action_group;
  GCancellable *cancellable;
};

G_DEFINE_TYPE (EphyAutoFillView, ephy_autofill_view, ADW_TYPE_NAVIGATION_PAGE)


typedef struct {
  char *code;
  char *name;
} Mapping;

static Mapping country_map[] = {
  { "AF", "Afghanistan" },
  { "AX", "Åland Islands" },
  { "AL", "Albania" },
  { "DZ", "Algeria" },
  { "AS", "American Samoa" },
  { "AD", "Andorra" },
  { "AO", "Angola" },
  { "AI", "Anguilla" },
  { "AQ", "Antarctica" },
  { "AG", "Antigua and Barbuda" },
  { "AR", "Argentina" },
  { "AM", "Armenia" },
  { "AW", "Aruba" },
  { "AU", "Australia" },
  { "AT", "Austria" },
  { "AZ", "Azerbaijan" },
  { "BS", "Bahamas" },
  { "BH", "Bahrain" },
  { "BD", "Bangladesh" },
  { "BB", "Barbados" },
  { "BY", "Belarus" },
  { "BE", "Belgium" },
  { "BZ", "Belize" },
  { "BJ", "Benin" },
  { "BM", "Bermuda" },
  { "BT", "Bhutan" },
  { "BO", "Bolivia" },
  { "BA", "Bosnia and Herzegovina" },
  { "BW", "Botswana" },
  { "BV", "Bouvet Island" },
  { "BR", "Brazil" },
  { "IO", "British Indian Ocean Territory" },
  { "BN", "Brunei Darussalam" },
  { "BG", "Bulgaria" },
  { "BF", "Burkina Faso" },
  { "BI", "Burundi" },
  { "KH", "Cambodia" },
  { "CM", "Cameroon" },
  { "CA", "Canada" },
  { "CV", "Cape Verde" },
  { "KY", "Cayman Islands" },
  { "CF", "Central African Republic" },
  { "TD", "Chad" },
  { "CL", "Chile" },
  { "CN", "China" },
  { "CX", "Christmas Island" },
  { "CC", "Cocos (Keeling) Islands" },
  { "CO", "Colombia" },
  { "KM", "Comoros" },
  { "CG", "Congo" },
  { "CD", "Congo, Democratic Republic" },
  { "CK", "Cook Islands" },
  { "CR", "Costa Rica" },
  { "CI", "Cote D'Ivoire" },
  { "HR", "Croatia" },
  { "CU", "Cuba" },
  { "CY", "Cyprus" },
  { "CZ", "Czech Republic" },
  { "DK", "Denmark" },
  { "DJ", "Djibouti" },
  { "DM", "Dominica" },
  { "DO", "Dominican Republic" },
  { "EC", "Ecuador" },
  { "EG", "Egypt" },
  { "SV", "El Salvador" },
  { "GQ", "Equatorial Guinea" },
  { "ER", "Eritrea" },
  { "EE", "Estonia" },
  { "ET", "Ethiopia" },
  { "FK", "Falkland Islands (Malvinas)" },
  { "FO", "Faroe Islands" },
  { "FJ", "Fiji" },
  { "FI", "Finland" },
  { "FR", "France" },
  { "GF", "French Guiana" },
  { "PF", "French Polynesia" },
  { "TF", "French Southern Territories" },
  { "GA", "Gabon" },
  { "GM", "Gambia" },
  { "GE", "Georgia" },
  { "DE", "Germany" },
  { "GH", "Ghana" },
  { "GI", "Gibraltar" },
  { "GR", "Greece" },
  { "GL", "Greenland" },
  { "GD", "Grenada" },
  { "GP", "Guadeloupe" },
  { "GU", "Guam" },
  { "GT", "Guatemala" },
  { "GG", "Guernsey" },
  { "GN", "Guinea" },
  { "GW", "Guinea-Bissau" },
  { "GY", "Guyana" },
  { "HT", "Haiti" },
  { "HM", "Heard Island and Mcdonald Islands" },
  { "VA", "Holy See (Vatican City State)" },
  { "HN", "Honduras" },
  { "HK", "Hong Kong" },
  { "HU", "Hungary" },
  { "IS", "Iceland" },
  { "IN", "India" },
  { "ID", "Indonesia" },
  { "IR", "Iran" },
  { "IQ", "Iraq" },
  { "IE", "Ireland" },
  { "IM", "Isle of Man" },
  { "IL", "Israel" },
  { "IT", "Italy" },
  { "JM", "Jamaica" },
  { "JP", "Japan" },
  { "JE", "Jersey" },
  { "JO", "Jordan" },
  { "KZ", "Kazakhstan" },
  { "KE", "Kenya" },
  { "KI", "Kiribati" },
  { "KP", "Korea (North)" },
  { "KR", "Korea (South)" },
  { "XK", "Kosovo" },
  { "KW", "Kuwait" },
  { "KG", "Kyrgyzstan" },
  { "LA", "Laos" },
  { "LV", "Latvia" },
  { "LB", "Lebanon" },
  { "LS", "Lesotho" },
  { "LR", "Liberia" },
  { "LY", "Libyan Arab Jamahiriya" },
  { "LI", "Liechtenstein" },
  { "LT", "Lithuania" },
  { "LU", "Luxembourg" },
  { "MO", "Macao" },
  { "MK", "Macedonia" },
  { "MG", "Madagascar" },
  { "MW", "Malawi" },
  { "MY", "Malaysia" },
  { "MV", "Maldives" },
  { "ML", "Mali" },
  { "MT", "Malta" },
  { "MH", "Marshall Islands" },
  { "MQ", "Martinique" },
  { "MR", "Mauritania" },
  { "MU", "Mauritius" },
  { "YT", "Mayotte" },
  { "MX", "Mexico" },
  { "FM", "Micronesia" },
  { "MD", "Moldova" },
  { "MC", "Monaco" },
  { "MN", "Mongolia" },
  { "MS", "Montserrat" },
  { "MA", "Morocco" },
  { "MZ", "Mozambique" },
  { "MM", "Myanmar" },
  { "NA", "Namibia" },
  { "NR", "Nauru" },
  { "NP", "Nepal" },
  { "NL", "Netherlands" },
  { "AN", "Netherlands Antilles" },
  { "NC", "New Caledonia" },
  { "NZ", "New Zealand" },
  { "NI", "Nicaragua" },
  { "NE", "Niger" },
  { "NG", "Nigeria" },
  { "NU", "Niue" },
  { "NF", "Norfolk Island" },
  { "MP", "Northern Mariana Islands" },
  { "NO", "Norway" },
  { "OM", "Oman" },
  { "PK", "Pakistan" },
  { "PW", "Palau" },
  { "PS", "Palestinian Territory, Occupied" },
  { "PA", "Panama" },
  { "PG", "Papua New Guinea" },
  { "PY", "Paraguay" },
  { "PE", "Peru" },
  { "PH", "Philippines" },
  { "PN", "Pitcairn" },
  { "PL", "Poland" },
  { "PT", "Portugal" },
  { "PR", "Puerto Rico" },
  { "QA", "Qatar" },
  { "RE", "Reunion" },
  { "RO", "Romania" },
  { "RU", "Russian Federation" },
  { "RW", "Rwanda" },
  { "SH", "Saint Helena" },
  { "KN", "Saint Kitts and Nevis" },
  { "LC", "Saint Lucia" },
  { "PM", "Saint Pierre and Miquelon" },
  { "VC", "Saint Vincent and the Grenadines" },
  { "WS", "Samoa" },
  { "SM", "San Marino" },
  { "ST", "Sao Tome and Principe" },
  { "SA", "Saudi Arabia" },
  { "SN", "Senegal" },
  { "RS", "Serbia" },
  { "ME", "Montenegro" },
  { "SC", "Seychelles" },
  { "SL", "Sierra Leone" },
  { "SG", "Singapore" },
  { "SK", "Slovakia" },
  { "SI", "Slovenia" },
  { "SB", "Solomon Islands" },
  { "SO", "Somalia" },
  { "ZA", "South Africa" },
  { "GS", "South Georgia and the South Sandwich Islands" },
  { "ES", "Spain" },
  { "LK", "Sri Lanka" },
  { "SD", "Sudan" },
  { "SR", "Suriname" },
  { "SJ", "Svalbard and Jan Mayen" },
  { "SZ", "Swaziland" },
  { "SE", "Sweden" },
  { "CH", "Switzerland" },
  { "SY", "Syrian Arab Republic" },
  { "TW", "Taiwan, Province of China" },
  { "TJ", "Tajikistan" },
  { "TZ", "Tanzania" },
  { "TH", "Thailand" },
  { "TL", "Timor-Leste" },
  { "TG", "Togo" },
  { "TK", "Tokelau" },
  { "TO", "Tonga" },
  { "TT", "Trinidad and Tobago" },
  { "TN", "Tunisia" },
  { "TR", "Turkey" },
  { "TM", "Turkmenistan" },
  { "TC", "Turks and Caicos Islands" },
  { "TV", "Tuvalu" },
  { "UG", "Uganda" },
  { "UA", "Ukraine" },
  { "AE", "United Arab Emirates" },
  { "GB", "United Kingdom" },
  { "US", "United States" },
  { "UM", "United States Minor Outlying Islands" },
  { "UY", "Uruguay" },
  { "UZ", "Uzbekistan" },
  { "VU", "Vanuatu" },
  { "VE", "Venezuela" },
  { "VN", "Viet Nam" },
  { "VG", "Virgin Islands, British" },
  { "VI", "Virgin Islands, U.S." },
  { "WF", "Wallis and Futuna" },
  { "EH", "Western Sahara" },
  { "YE", "Yemen" },
  { "ZM", "Zambia" },
  { "ZW", "Zimbabwe" },
  { NULL, NULL }
};

static Mapping card_map[] = {
  { "A", "American Express" },
  { "M", "Mastercard" },
  { "D", "Discover" },
  { "V", "Visa" },
  { NULL, NULL }
};

/* Currently not implemented in UI: */
/* * @EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH_MM: */
/* * @EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH_M: */
/* * @EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH: */
/* * @EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR_YYYY: */
/* * @EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR_YY: */
/* * @EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR: */
/* * @EPHY_AUTOFILL_FIELD_CARD_EXPDATE: */
/* * @EPHY_AUTOFILL_FIELD_CARD_TYPE: */

static void
confirmation_dialog_response_cb (EphyAutoFillView *self)
{
  gtk_editable_set_text (GTK_EDITABLE (self->first_name), "");
  gtk_editable_set_text (GTK_EDITABLE (self->last_name), "");
  gtk_editable_set_text (GTK_EDITABLE (self->full_name), "");
  gtk_editable_set_text (GTK_EDITABLE (self->user_name), "");
  gtk_editable_set_text (GTK_EDITABLE (self->email), "");
  gtk_editable_set_text (GTK_EDITABLE (self->phone), "");
  gtk_editable_set_text (GTK_EDITABLE (self->street), "");
  gtk_editable_set_text (GTK_EDITABLE (self->organization), "");
  gtk_editable_set_text (GTK_EDITABLE (self->postal_code), "");
  gtk_editable_set_text (GTK_EDITABLE (self->state), "");
  gtk_editable_set_text (GTK_EDITABLE (self->city), "");
  gtk_editable_set_text (GTK_EDITABLE (self->card_owner), "");
  gtk_editable_set_text (GTK_EDITABLE (self->card_number), "");

  adw_combo_row_set_selected (ADW_COMBO_ROW (self->country), 0);
  adw_combo_row_set_selected (ADW_COMBO_ROW (self->card_type), 0);
}

static GtkWidget *
confirmation_dialog_construct (EphyAutoFillView *self)
{
  AdwDialog *dialog;

  dialog = adw_alert_dialog_new (_("Delete All Entries?"),
                                 _("This will clear all locally stored entries, and can not be undone."));

  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                  "cancel", _("_Cancel"),
                                  "delete", _("_Delete"),
                                  NULL);
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog),
                                            "delete",
                                            ADW_RESPONSE_DESTRUCTIVE);

  g_signal_connect_swapped (dialog, "response::delete",
                            G_CALLBACK (confirmation_dialog_response_cb),
                            self);

  return GTK_WIDGET (dialog);
}

static void
on_clear_all (GtkWidget *widget,
              gpointer   user_data)

{
  EphyAutoFillView *self = EPHY_AUTOFILL_VIEW (user_data);
  GtkWidget *window = gtk_widget_get_parent (GTK_WIDGET (self));

  self->confirmation_dialog = confirmation_dialog_construct (self);
  adw_dialog_present (ADW_DIALOG (self->confirmation_dialog), window);
}

static void
ephy_autofill_view_dispose (GObject *object)
{
  EphyAutoFillView *self = (EphyAutoFillView *)object;

  if (self->cancellable) {
    g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);
  }

  G_OBJECT_CLASS (ephy_autofill_view_parent_class)->dispose (object);
}

static void
ephy_autofill_view_class_init (EphyAutoFillViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/autofill-view.ui");

  object_class->dispose = ephy_autofill_view_dispose;

  gtk_widget_class_bind_template_callback (widget_class, on_clear_all);

  gtk_widget_class_bind_template_child (widget_class, EphyAutoFillView, first_name);
  gtk_widget_class_bind_template_child (widget_class, EphyAutoFillView, last_name);
  gtk_widget_class_bind_template_child (widget_class, EphyAutoFillView, full_name);
  gtk_widget_class_bind_template_child (widget_class, EphyAutoFillView, user_name);
  gtk_widget_class_bind_template_child (widget_class, EphyAutoFillView, email);
  gtk_widget_class_bind_template_child (widget_class, EphyAutoFillView, phone);
  gtk_widget_class_bind_template_child (widget_class, EphyAutoFillView, street);
  gtk_widget_class_bind_template_child (widget_class, EphyAutoFillView, organization);
  gtk_widget_class_bind_template_child (widget_class, EphyAutoFillView, postal_code);
  gtk_widget_class_bind_template_child (widget_class, EphyAutoFillView, state);
  gtk_widget_class_bind_template_child (widget_class, EphyAutoFillView, city);
  gtk_widget_class_bind_template_child (widget_class, EphyAutoFillView, country);
  gtk_widget_class_bind_template_child (widget_class, EphyAutoFillView, card_type);
  gtk_widget_class_bind_template_child (widget_class, EphyAutoFillView, card_owner);
  gtk_widget_class_bind_template_child (widget_class, EphyAutoFillView, card_number);
}

void
prefs_autofill_utils_get_entry_cb (GObject      *source_object,
                                   GAsyncResult *res,
                                   gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  GtkWidget *row = user_data;
  g_autofree char *autofill_value = ephy_autofill_storage_get_finish (res, &error);

  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Could not get autofill storage data: %s", error->message);
  } else if (autofill_value) {
    gtk_editable_set_text (GTK_EDITABLE (row), autofill_value);
  }
}

void
prefs_autofill_utils_get_country_cb (GObject      *source_object,
                                     GAsyncResult *res,
                                     gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  EphyAutoFillView *self = EPHY_AUTOFILL_VIEW (user_data);
  g_autofree char *autofill_value = ephy_autofill_storage_get_finish (res, &error);

  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Could not get autofill storage data: %s", error->message);
  } else if (autofill_value) {
    for (int i = 0; country_map[i].name; i++) {
      if (g_strcmp0 (country_map[i].name, autofill_value) == 0) {
        adw_combo_row_set_selected (ADW_COMBO_ROW (self->country), i);
        break;
      }
    }
  }
}

void
prefs_autofill_utils_get_card_cb (GObject      *source_object,
                                  GAsyncResult *res,
                                  gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  EphyAutoFillView *self = EPHY_AUTOFILL_VIEW (user_data);
  g_autofree char *autofill_value = ephy_autofill_storage_get_finish (res, &error);

  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Could not get autofill storage data: %s", error->message);
  } else if (autofill_value) {
    for (int i = 0; card_map[i].name; i++) {
      if (g_strcmp0 (card_map[i].name, autofill_value) == 0) {
        adw_combo_row_set_selected (ADW_COMBO_ROW (self->card_type), i);
        break;
      }
    }
  }
}

static void
on_entry_changed (GtkEditable *widget,
                  gpointer     user_data)
{
  EphyAutofillField field = GPOINTER_TO_INT (user_data);
  const char *text = gtk_editable_get_text (GTK_EDITABLE (widget));

  ephy_autofill_storage_set (field, text, NULL, NULL, NULL);
}

static void
on_country_selected (GObject    *gobject,
                     GParamSpec *pspec,
                     gpointer    user_data)
{
  int pos = adw_combo_row_get_selected (ADW_COMBO_ROW (gobject));
  ephy_autofill_storage_set (EPHY_AUTOFILL_FIELD_COUNTRY, country_map[pos].name, NULL, NULL, NULL);
  ephy_autofill_storage_set (EPHY_AUTOFILL_FIELD_COUNTRY_CODE, country_map[pos].code, NULL, NULL, NULL);
}

static void
on_card_selected (GObject    *gobject,
                  GParamSpec *pspec,
                  gpointer    user_data)
{
  int pos = adw_combo_row_get_selected (ADW_COMBO_ROW (gobject));
  ephy_autofill_storage_set (EPHY_AUTOFILL_FIELD_CARD_TYPE_NAME, card_map[pos].name, NULL, NULL, NULL);
  ephy_autofill_storage_set (EPHY_AUTOFILL_FIELD_CARD_TYPE_CODE, card_map[pos].code, NULL, NULL, NULL);
}

static void
ephy_autofill_view_init (EphyAutoFillView *self)
{
  GtkStringList *country_model;
  GtkStringList *card_model;

  gtk_widget_init_template (GTK_WIDGET (self));
  self->cancellable = g_cancellable_new ();

  ephy_autofill_storage_get (EPHY_AUTOFILL_FIELD_FIRSTNAME,
                             self->cancellable,
                             prefs_autofill_utils_get_entry_cb,
                             self->first_name);
  g_signal_connect (self->first_name, "changed", G_CALLBACK (on_entry_changed), GINT_TO_POINTER (EPHY_AUTOFILL_FIELD_FIRSTNAME));

  ephy_autofill_storage_get (EPHY_AUTOFILL_FIELD_LASTNAME,
                             self->cancellable,
                             prefs_autofill_utils_get_entry_cb,
                             self->last_name);
  g_signal_connect (self->last_name, "changed", G_CALLBACK (on_entry_changed), GINT_TO_POINTER (EPHY_AUTOFILL_FIELD_LASTNAME));

  ephy_autofill_storage_get (EPHY_AUTOFILL_FIELD_FULLNAME,
                             self->cancellable,
                             prefs_autofill_utils_get_entry_cb,
                             self->full_name);
  g_signal_connect (self->full_name, "changed", G_CALLBACK (on_entry_changed), GINT_TO_POINTER (EPHY_AUTOFILL_FIELD_FULLNAME));

  ephy_autofill_storage_get (EPHY_AUTOFILL_FIELD_USERNAME,
                             self->cancellable,
                             prefs_autofill_utils_get_entry_cb,
                             self->user_name);
  g_signal_connect (self->user_name, "changed", G_CALLBACK (on_entry_changed), GINT_TO_POINTER (EPHY_AUTOFILL_FIELD_USERNAME));

  ephy_autofill_storage_get (EPHY_AUTOFILL_FIELD_EMAIL,
                             self->cancellable,
                             prefs_autofill_utils_get_entry_cb,
                             self->email);
  g_signal_connect (self->email, "changed", G_CALLBACK (on_entry_changed), GINT_TO_POINTER (EPHY_AUTOFILL_FIELD_EMAIL));

  ephy_autofill_storage_get (EPHY_AUTOFILL_FIELD_PHONE,
                             self->cancellable,
                             prefs_autofill_utils_get_entry_cb,
                             self->phone);
  g_signal_connect (self->phone, "changed", G_CALLBACK (on_entry_changed), GINT_TO_POINTER (EPHY_AUTOFILL_FIELD_PHONE));

  ephy_autofill_storage_get (EPHY_AUTOFILL_FIELD_STREET_ADDRESS,
                             self->cancellable,
                             prefs_autofill_utils_get_entry_cb,
                             self->street);
  g_signal_connect (self->street, "changed", G_CALLBACK (on_entry_changed), GINT_TO_POINTER (EPHY_AUTOFILL_FIELD_STREET_ADDRESS));

  ephy_autofill_storage_get (EPHY_AUTOFILL_FIELD_ORGANIZATION,
                             self->cancellable,
                             prefs_autofill_utils_get_entry_cb,
                             self->organization);
  g_signal_connect (self->organization, "changed", G_CALLBACK (on_entry_changed), GINT_TO_POINTER (EPHY_AUTOFILL_FIELD_ORGANIZATION));

  ephy_autofill_storage_get (EPHY_AUTOFILL_FIELD_POSTAL_CODE,
                             self->cancellable,
                             prefs_autofill_utils_get_entry_cb,
                             self->postal_code);
  g_signal_connect (self->postal_code, "changed", G_CALLBACK (on_entry_changed), GINT_TO_POINTER (EPHY_AUTOFILL_FIELD_POSTAL_CODE));

  ephy_autofill_storage_get (EPHY_AUTOFILL_FIELD_STATE,
                             self->cancellable,
                             prefs_autofill_utils_get_entry_cb,
                             self->state);
  g_signal_connect (self->state, "changed", G_CALLBACK (on_entry_changed), GINT_TO_POINTER (EPHY_AUTOFILL_FIELD_STATE));

  ephy_autofill_storage_get (EPHY_AUTOFILL_FIELD_CITY,
                             self->cancellable,
                             prefs_autofill_utils_get_entry_cb,
                             self->city);
  g_signal_connect (self->city, "changed", G_CALLBACK (on_entry_changed), GINT_TO_POINTER (EPHY_AUTOFILL_FIELD_CITY));

  country_model = gtk_string_list_new (NULL);

  for (int i = 0; country_map[i].code; i++)
    gtk_string_list_append (country_model, country_map[i].name);

  adw_combo_row_set_model (ADW_COMBO_ROW (self->country), G_LIST_MODEL (country_model));
  ephy_autofill_storage_get (EPHY_AUTOFILL_FIELD_COUNTRY,
                             self->cancellable,
                             prefs_autofill_utils_get_country_cb,
                             self);
  g_signal_connect (self->country, "notify::selected-item", G_CALLBACK (on_country_selected), self);

  card_model = gtk_string_list_new (NULL);

  for (int i = 0; card_map[i].code; i++)
    gtk_string_list_append (card_model, card_map[i].name);

  adw_combo_row_set_model (ADW_COMBO_ROW (self->card_type), G_LIST_MODEL (card_model));
  ephy_autofill_storage_get (EPHY_AUTOFILL_FIELD_CARD_TYPE,
                             self->cancellable,
                             prefs_autofill_utils_get_card_cb,
                             self);
  g_signal_connect (self->card_type, "notify::selected-item", G_CALLBACK (on_card_selected), self);

  ephy_autofill_storage_get (EPHY_AUTOFILL_FIELD_NAME_ON_CARD,
                             self->cancellable,
                             prefs_autofill_utils_get_entry_cb,
                             self->card_owner);
  g_signal_connect (self->card_owner, "changed", G_CALLBACK (on_entry_changed), GINT_TO_POINTER (EPHY_AUTOFILL_FIELD_NAME_ON_CARD));

  ephy_autofill_storage_get (EPHY_AUTOFILL_FIELD_CARD_NUMBER,
                             self->cancellable,
                             prefs_autofill_utils_get_entry_cb,
                             self->card_number);
  g_signal_connect (self->card_number, "changed", G_CALLBACK (on_entry_changed), GINT_TO_POINTER (EPHY_AUTOFILL_FIELD_CARD_NUMBER));
}
