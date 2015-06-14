/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2018 Abdullah Alansari
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
#include "prefs-autofill-dialog.h"

#include "ephy-autofill.h"
#include "ephy-autofill-field.h"
#include "prefs-autofill-utils.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <string.h>

struct _PrefsAutofillDialog
{
  GtkDialog parent_instance;

  GtkEntry *firstname_entry;
  GtkEntry *lastname_entry;
  GtkEntry *username_entry;
  GtkEntry *email_entry;
  GtkEntry *phone_entry;

  GtkComboBoxText *country_combo_box_text;
  GtkEntry *street_address_entry;
  GtkEntry *organization_entry;
  GtkEntry *postal_code_entry;
  GtkEntry *state_entry;
  GtkEntry *city_entry;

  GtkComboBoxText *expdate_month_combo_box_text;
  GtkComboBoxText *expdate_year_combo_box_text;
  GtkComboBoxText *card_type_combo_box_text;
  GtkEntry *name_on_card_entry;
  GtkEntry *card_number_entry;
};

G_DEFINE_TYPE (PrefsAutofillDialog, prefs_autofill_dialog, GTK_TYPE_DIALOG)

PrefsAutofillDialog *
prefs_autofill_dialog_new (void) {
  return g_object_new (EPHY_TYPE_PREFS_AUTOFILL_DIALOG,
                       "use-header-bar", TRUE, NULL);
}

static bool
is_empty_value (const char *value) {
  return value == NULL || strlen (value) == 0;
}

static void
clear_personal_data (PrefsAutofillDialog *dialog)
{
  gtk_entry_set_text (dialog->firstname_entry, "");
  gtk_entry_set_text (dialog->lastname_entry, "");
  gtk_entry_set_text (dialog->username_entry, "");
  gtk_entry_set_text (dialog->email_entry, "");
  gtk_entry_set_text (dialog->phone_entry, "");

  gtk_combo_box_set_active_id (GTK_COMBO_BOX (dialog->country_combo_box_text), "US");
  gtk_entry_set_text (dialog->street_address_entry, "");
  gtk_entry_set_text (dialog->organization_entry, "");
  gtk_entry_set_text (dialog->postal_code_entry, "");
  gtk_entry_set_text (dialog->state_entry, "");
  gtk_entry_set_text (dialog->city_entry, "");
}

static void
clear_card_data (PrefsAutofillDialog *dialog)
{
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (dialog->expdate_year_combo_box_text), "17");
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (dialog->expdate_month_combo_box_text), "1");
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (dialog->card_type_combo_box_text), "A");

  gtk_entry_set_text (dialog->name_on_card_entry, "");
  gtk_entry_set_text (dialog->card_number_entry, "");
}

static void
clear_all (PrefsAutofillDialog *dialog)
{
  clear_personal_data (dialog);
  clear_card_data (dialog);
}

static void
save_or_delete (EphyAutofillField field,
                const char *value)
{
  if (is_empty_value (value))
    ephy_autofill_delete (field, NULL, NULL);
  else
    ephy_autofill_set (field, value, NULL, NULL);
}

static void
save_personal_data (PrefsAutofillDialog *dialog)
{
  const char *firstname = gtk_entry_get_text (dialog->firstname_entry);
  const char *lastname = gtk_entry_get_text (dialog->lastname_entry);
  char *fullname = (is_empty_value (firstname) || is_empty_value (lastname)) ?
    NULL : g_strjoin (" ", firstname, lastname, NULL);

  const char *country_code = gtk_combo_box_get_active_id (GTK_COMBO_BOX (dialog->country_combo_box_text));
  char *country_name = gtk_combo_box_text_get_active_text (dialog->country_combo_box_text);

  save_or_delete (EPHY_AUTOFILL_FIELD_FIRSTNAME, firstname);
  save_or_delete (EPHY_AUTOFILL_FIELD_LASTNAME, lastname);

  save_or_delete (EPHY_AUTOFILL_FIELD_USERNAME, gtk_entry_get_text (dialog->username_entry));
  save_or_delete (EPHY_AUTOFILL_FIELD_EMAIL, gtk_entry_get_text (dialog->email_entry));
  save_or_delete (EPHY_AUTOFILL_FIELD_PHONE, gtk_entry_get_text (dialog->phone_entry));

  save_or_delete (EPHY_AUTOFILL_FIELD_STREET_ADDRESS, gtk_entry_get_text (dialog->street_address_entry));
  save_or_delete (EPHY_AUTOFILL_FIELD_ORGANIZATION, gtk_entry_get_text (dialog->organization_entry));
  save_or_delete (EPHY_AUTOFILL_FIELD_POSTAL_CODE, gtk_entry_get_text (dialog->postal_code_entry));
  save_or_delete (EPHY_AUTOFILL_FIELD_STATE, gtk_entry_get_text (dialog->state_entry));
  save_or_delete (EPHY_AUTOFILL_FIELD_CITY, gtk_entry_get_text (dialog->city_entry));

  if (is_empty_value (fullname))
    ephy_autofill_delete (EPHY_AUTOFILL_FIELD_FULLNAME, NULL, NULL);
  else
    ephy_autofill_set (EPHY_AUTOFILL_FIELD_FULLNAME, fullname,
                       prefs_autofill_utils_set_free_cb, fullname);

  ephy_autofill_set (EPHY_AUTOFILL_FIELD_COUNTRY_CODE, country_code, NULL, NULL);
  ephy_autofill_set (EPHY_AUTOFILL_FIELD_COUNTRY_NAME, country_name,
                     prefs_autofill_utils_set_free_cb, country_name);
}

static void
save_card_data (PrefsAutofillDialog *dialog)
{
  const char *year_yy = prefs_autofill_utils_get_active_id (dialog->expdate_year_combo_box_text);
  const char *month_m = prefs_autofill_utils_get_active_id (dialog->expdate_month_combo_box_text);

  char *card_type_name = gtk_combo_box_text_get_active_text (dialog->card_type_combo_box_text);
  char *year_yyyy = gtk_combo_box_text_get_active_text (dialog->expdate_year_combo_box_text);
  char *month_mm = gtk_combo_box_text_get_active_text (dialog->expdate_month_combo_box_text);

  char *expdate_mm_yyyy = (year_yy == NULL || month_mm == NULL) ? NULL : g_strjoin ("/", month_mm, year_yy, NULL);

  ephy_autofill_set (EPHY_AUTOFILL_FIELD_CARD_EXPDATE, expdate_mm_yyyy,
                     prefs_autofill_utils_set_free_cb, expdate_mm_yyyy);

  ephy_autofill_set (EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH_M, month_m, NULL, NULL);
  ephy_autofill_set (EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR_YY, year_yy, NULL, NULL);

  ephy_autofill_set (EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR_YYYY, year_yyyy,
                     prefs_autofill_utils_set_free_cb, year_yyyy);

  ephy_autofill_set (EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH_MM, month_mm,
                     prefs_autofill_utils_set_free_cb, month_mm);

  save_or_delete (EPHY_AUTOFILL_FIELD_NAME_ON_CARD, gtk_entry_get_text (dialog->name_on_card_entry));
  save_or_delete (EPHY_AUTOFILL_FIELD_CARD_NUMBER, gtk_entry_get_text (dialog->card_number_entry));

  ephy_autofill_set (EPHY_AUTOFILL_FIELD_CARD_TYPE_CODE,
                     prefs_autofill_utils_get_active_id (dialog->card_type_combo_box_text),
                     NULL, NULL);

  ephy_autofill_set (EPHY_AUTOFILL_FIELD_CARD_TYPE_NAME, card_type_name,
                     prefs_autofill_utils_set_free_cb, card_type_name);
}

static void
response_cb (PrefsAutofillDialog *dialog,
             GtkResponseType response,
             gpointer *data)
{
  if (response == GTK_RESPONSE_DELETE_EVENT) {
    save_personal_data (dialog);
    save_card_data (dialog);
    gtk_widget_destroy (GTK_WIDGET (dialog));
  }
  else if (response == GTK_RESPONSE_CANCEL)
    clear_all (dialog);
}

static void
init_personal_data (PrefsAutofillDialog *dialog)
{
  ephy_autofill_get (EPHY_AUTOFILL_FIELD_FIRSTNAME,
                     prefs_autofill_utils_get_entry_cb,
                     dialog->firstname_entry);

  ephy_autofill_get (EPHY_AUTOFILL_FIELD_LASTNAME,
                     prefs_autofill_utils_get_entry_cb,
                     dialog->lastname_entry);

  ephy_autofill_get (EPHY_AUTOFILL_FIELD_USERNAME,
                     prefs_autofill_utils_get_entry_cb,
                     dialog->username_entry);

  ephy_autofill_get (EPHY_AUTOFILL_FIELD_EMAIL,
                     prefs_autofill_utils_get_entry_cb,
                     dialog->email_entry);

  ephy_autofill_get (EPHY_AUTOFILL_FIELD_PHONE,
                     prefs_autofill_utils_get_entry_cb,
                     dialog->phone_entry);


  ephy_autofill_get (EPHY_AUTOFILL_FIELD_STREET_ADDRESS,
                     prefs_autofill_utils_get_entry_cb,
                     dialog->street_address_entry);

  ephy_autofill_get (EPHY_AUTOFILL_FIELD_ORGANIZATION,
                     prefs_autofill_utils_get_entry_cb,
                     dialog->organization_entry);

  ephy_autofill_get (EPHY_AUTOFILL_FIELD_POSTAL_CODE,
                     prefs_autofill_utils_get_entry_cb,
                     dialog->postal_code_entry);

  ephy_autofill_get (EPHY_AUTOFILL_FIELD_COUNTRY_CODE,
                     prefs_autofill_utils_get_combo_box_text_cb,
                     dialog->country_combo_box_text);

  ephy_autofill_get (EPHY_AUTOFILL_FIELD_STATE,
                     prefs_autofill_utils_get_entry_cb,
                     dialog->state_entry);

  ephy_autofill_get (EPHY_AUTOFILL_FIELD_CITY,
                     prefs_autofill_utils_get_entry_cb,
                     dialog->city_entry);
}

static void
init_card_data (PrefsAutofillDialog *dialog)
{
  ephy_autofill_get (EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH_M,
                     prefs_autofill_utils_get_combo_box_text_cb,
                     dialog->expdate_month_combo_box_text);

  ephy_autofill_get (EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR_YY,
                     prefs_autofill_utils_get_combo_box_text_cb,
                     dialog->expdate_year_combo_box_text);

  ephy_autofill_get (EPHY_AUTOFILL_FIELD_NAME_ON_CARD,
                     prefs_autofill_utils_get_entry_cb,
                     dialog->name_on_card_entry);

  ephy_autofill_get (EPHY_AUTOFILL_FIELD_CARD_NUMBER,
                     prefs_autofill_utils_get_entry_cb,
                     dialog->card_number_entry);

  ephy_autofill_get (EPHY_AUTOFILL_FIELD_CARD_TYPE_CODE,
                     prefs_autofill_utils_get_combo_box_text_cb,
                     dialog->card_type_combo_box_text);
}

static void
prefs_autofill_dialog_init (PrefsAutofillDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));

  init_personal_data (dialog);
  init_card_data (dialog);
}

static void
class_init_card_fields (GtkWidgetClass *widget_class)
{
  gtk_widget_class_bind_template_child (widget_class, PrefsAutofillDialog, expdate_month_combo_box_text);
  gtk_widget_class_bind_template_child (widget_class, PrefsAutofillDialog, expdate_year_combo_box_text);
  gtk_widget_class_bind_template_child (widget_class, PrefsAutofillDialog, card_type_combo_box_text);
  gtk_widget_class_bind_template_child (widget_class, PrefsAutofillDialog, name_on_card_entry);
  gtk_widget_class_bind_template_child (widget_class, PrefsAutofillDialog, card_number_entry);
}

static void
class_init_personal_fields (GtkWidgetClass *widget_class)
{
  gtk_widget_class_bind_template_child (widget_class, PrefsAutofillDialog, firstname_entry);
  gtk_widget_class_bind_template_child (widget_class, PrefsAutofillDialog, lastname_entry);
  gtk_widget_class_bind_template_child (widget_class, PrefsAutofillDialog, username_entry);
  gtk_widget_class_bind_template_child (widget_class, PrefsAutofillDialog, email_entry);
  gtk_widget_class_bind_template_child (widget_class, PrefsAutofillDialog, phone_entry);

  gtk_widget_class_bind_template_child (widget_class, PrefsAutofillDialog, country_combo_box_text);
  gtk_widget_class_bind_template_child (widget_class, PrefsAutofillDialog, street_address_entry);
  gtk_widget_class_bind_template_child (widget_class, PrefsAutofillDialog, organization_entry);
  gtk_widget_class_bind_template_child (widget_class, PrefsAutofillDialog, postal_code_entry);
  gtk_widget_class_bind_template_child (widget_class, PrefsAutofillDialog, state_entry);
  gtk_widget_class_bind_template_child (widget_class, PrefsAutofillDialog, city_entry);
}

static void
prefs_autofill_dialog_class_init (PrefsAutofillDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/prefs-autofill-dialog.ui");

  class_init_personal_fields (widget_class);
  class_init_card_fields (widget_class);

  gtk_widget_class_bind_template_callback (widget_class, response_cb);
}
