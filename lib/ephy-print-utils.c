/*
 *  Copyright © 2006 Christian Persch
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "config.h"

#include <string.h>

#include <glib.h>
#include <gtk/gtktypebuiltins.h>

#include "ephy-debug.h"
#include "ephy-string.h"

#include "ephy-print-utils.h"

#define PRINT_SETTINGS_GROUP	"Print Settings"
#define PAGE_SETUP_GROUP	"Page Setup"
#define PAPER_SIZE_GROUP	"Paper Size"

#define ERROR_QUARK		(g_quark_from_static_string ("ephy-print-utils-error"))

/**
 * ephy_print_utils_settings_new_from_file:
 * @file_name: the filename to read the settings from
 * @error:
 * 
 * Reads the print settings from @filename. Returns a new #GtkPrintSettings
 * object with the restored settings, or %NULL if an error occurred.
 *
 * Return value: the restored #GtkPrintSettings
 * 
 * Since: 2.10
 */
GtkPrintSettings *
ephy_print_utils_settings_new_from_file (const gchar *file_name,
					 GError     **error)
{
  GtkPrintSettings *settings;
  GKeyFile *key_file;

  g_return_val_if_fail (file_name != NULL, NULL);

  key_file = g_key_file_new ();
  if (!g_key_file_load_from_file (key_file, file_name, 0, error))
    {
      g_key_file_free (key_file);
      return NULL;
    }

  settings = ephy_print_utils_settings_new_from_key_file (key_file, error);
  g_key_file_free (key_file);

  return settings;
}

/**
 * ephy_print_utils_settings_new_from_key_file:
 * @key_file: the #GKeyFile to retrieve the settings from
 * @error:
 * 
 * Reads the print settings from @key_file. Returns a new #GtkPrintSettings
 * object with the restored settings, or %NULL if an error occurred.
 *
 * Return value: the restored #GtkPrintSettings
 * 
 * Since: 2.10
 */
GtkPrintSettings *
ephy_print_utils_settings_new_from_key_file (GKeyFile *key_file,
					     GError  **error)
{
  GtkPrintSettings *settings;
  gchar **keys;
  gsize n_keys, i;
  GError *err = NULL;

  g_return_val_if_fail (key_file != NULL, NULL);

  keys = g_key_file_get_keys (key_file,
			      PRINT_SETTINGS_GROUP,
			      &n_keys,
			      &err);
  if (err != NULL)
    {
      g_propagate_error (error, err);
      return NULL;
    }
   
  settings = gtk_print_settings_new ();

  for (i = 0 ; i < n_keys; ++i)
    {
      gchar *value;

      value = g_key_file_get_string (key_file,
				     PRINT_SETTINGS_GROUP,
				     keys[i],
				     NULL);
      if (!value)
        continue;

      gtk_print_settings_set (settings, keys[i], value);
      g_free (value);
    }

  g_strfreev (keys);

  return settings;
}

/**
 * ephy_print_utils_settings_to_file:
 * @settings: a #GtkPrintSettings
 * @file_name: the file to save to
 * @error:
 * 
 * This function saves the print settings from @settings to @file_name.
 * 
 * Return value: %TRUE on success
 *
 * Since: 2.10
 */
gboolean
ephy_print_utils_settings_to_file (GtkPrintSettings     *settings,
				   const char           *file_name,
				   GError              **error)
{
  GKeyFile *keyfile;
  gboolean retval;
  char *data = NULL;
  gsize len;
  GError *err = NULL;

  g_return_val_if_fail (GTK_IS_PRINT_SETTINGS (settings), FALSE);
  g_return_val_if_fail (file_name != NULL, FALSE);

  keyfile = g_key_file_new ();
  retval = ephy_print_utils_settings_to_key_file (settings, keyfile, &err);
  if (!retval) goto out;

  data = g_key_file_to_data (keyfile, &len, &err);
  if (!data) goto out;

  retval = g_file_set_contents (file_name, data, len, &err);

out:
  if (err != NULL)
    g_propagate_error (error, err);

  g_key_file_free (keyfile);
  g_free (data);

  return retval;
}

static void
add_value_to_keyfile (const gchar *key,
		      const gchar *value,
		      GKeyFile *keyfile)
{
  g_key_file_set_string (keyfile, PRINT_SETTINGS_GROUP, key, value);
}

/**
 * ephy_print_utils_settings_to_key_file:
 * @settings: a #GtkPrintSettings
 * @key_file: the #GKeyFile to save the print settings to
 * @error:
 * 
 * This function adds the print settings from @settings to @key_file.
 * 
 * Return value: %TRUE on success
 *
 * Since: 2.10
 */
gboolean
ephy_print_utils_settings_to_key_file (GtkPrintSettings  *settings,
				       GKeyFile          *key_file,
				       GError           **error)
{
  g_return_val_if_fail (GTK_IS_PRINT_SETTINGS (settings), FALSE);
  g_return_val_if_fail (key_file != NULL, FALSE);

  gtk_print_settings_foreach (settings,
			      (GtkPrintSettingsFunc) add_value_to_keyfile,
			      key_file);

  return TRUE;
}

/**
 * ephy_print_utils_page_setup_new_from_file:
 * @file_name: the filename to read the page_setup from
 * @error:
 * 
 * Reads the print page_setup from @filename. Returns a new #GtkPageSetup
 * object with the restored page_setup, or %NULL if an error occurred.
 *
 * Return value: the restored #GtkPageSetup
 * 
 * Since: 2.10
 */
GtkPageSetup *
ephy_print_utils_page_setup_new_from_file (const gchar *file_name,
					   GError     **error)
{
  GtkPageSetup *page_setup;
  GKeyFile *key_file;

  g_return_val_if_fail (file_name != NULL, NULL);

  key_file = g_key_file_new ();
  if (!g_key_file_load_from_file (key_file, file_name, 0, error))
    {
      g_key_file_free (key_file);
      return NULL;
    }

  page_setup = ephy_print_utils_page_setup_new_from_key_file (key_file, error);
  g_key_file_free (key_file);

  return page_setup;
}

/**
 * ephy_print_utils_page_setup_new_from_key_file:
 * @key_file: the #GKeyFile to retrieve the page_setup from
 * @error:
 * 
 * Reads the print page_setup from @key_file. Returns a new #GtkPageSetup
 * object with the restored page_setup, or %NULL if an error occurred.
 *
 * Return value: the restored #GtkPageSetup
 * 
 * Since: 2.10
 */
GtkPageSetup *
ephy_print_utils_page_setup_new_from_key_file (GKeyFile *key_file,
					       GError  **error)
{
  GtkPageSetup *page_setup = NULL;
  GtkPaperSize *paper_size;
  gdouble width, height, top, bottom, left, right;
  char *name = NULL, *ppd_name = NULL, *display_name = NULL, *orientation = NULL;
  gboolean retval = TRUE;
  GError *err = NULL;

  g_return_val_if_fail (key_file != NULL, NULL);

  if (!g_key_file_has_group (key_file, PAGE_SETUP_GROUP) ||
      !g_key_file_has_group (key_file, PAPER_SIZE_GROUP)) {
    g_set_error (error, ERROR_QUARK, 0, "Not a valid epiphany page setup file");
    retval = FALSE;
    goto out;
  }

#define GET_DOUBLE(kf, group, name, v) \
v = g_key_file_get_double (kf, group, name, &err); \
if (err != NULL) {\
  g_propagate_error (error, err);\
  retval = FALSE;\
  goto out;\
}

  GET_DOUBLE (key_file, PAPER_SIZE_GROUP, "Width", width);
  GET_DOUBLE (key_file, PAPER_SIZE_GROUP, "Height", height);
  GET_DOUBLE (key_file, PAGE_SETUP_GROUP, "MarginTop", top);
  GET_DOUBLE (key_file, PAGE_SETUP_GROUP, "MarginBottom", bottom);
  GET_DOUBLE (key_file, PAGE_SETUP_GROUP, "MarginLeft", left);
  GET_DOUBLE (key_file, PAGE_SETUP_GROUP, "MarginRight", right);

#undef GET_DOUBLE

  name = g_key_file_get_string (key_file, PAPER_SIZE_GROUP,
				"Name", NULL);
  ppd_name = g_key_file_get_string (key_file, PAPER_SIZE_GROUP,
				    "PPDName", NULL);
  display_name = g_key_file_get_string (key_file, PAPER_SIZE_GROUP,
					"DisplayName", NULL);
  orientation = g_key_file_get_string (key_file, PAGE_SETUP_GROUP,
				       "Orientation", NULL);

  if ((ppd_name == NULL && name == NULL) || orientation == NULL)
    {
      g_set_error (error, ERROR_QUARK, 0, "Not a valid epiphany page setup file");
      retval = FALSE;
      goto out;
    }

  if (ppd_name != NULL) {
    paper_size = gtk_paper_size_new_from_ppd (ppd_name, display_name,
					      width, height);
  } else {
    paper_size = gtk_paper_size_new_custom (name, display_name,
					    width, height, GTK_UNIT_MM);
  }
  g_assert (paper_size != NULL);

  page_setup = gtk_page_setup_new ();
  gtk_page_setup_set_paper_size (page_setup, paper_size);
  gtk_paper_size_free (paper_size);

  gtk_page_setup_set_top_margin (page_setup, top, GTK_UNIT_MM);
  gtk_page_setup_set_bottom_margin (page_setup, bottom, GTK_UNIT_MM);
  gtk_page_setup_set_left_margin (page_setup, left, GTK_UNIT_MM);
  gtk_page_setup_set_right_margin (page_setup, right, GTK_UNIT_MM);

  gtk_page_setup_set_orientation (page_setup,
				  ephy_string_enum_from_string (GTK_TYPE_PAGE_ORIENTATION,
						    		orientation));
out:
  g_free (ppd_name);
  g_free (name);
  g_free (display_name);
  g_free (orientation);

  return page_setup;
}

/**
 * ephy_print_utils_page_setup_to_file:
 * @page_setup: a #GtkPageSetup
 * @file_name: the file to save to
 * @error:
 * 
 * This function saves the print page_setup from @page_setup to @file_name.
 * 
 * Return value: %TRUE on success
 *
 * Since: 2.10
 */
gboolean
ephy_print_utils_page_setup_to_file (GtkPageSetup     *page_setup,
				     const char           *file_name,
				     GError              **error)
{
  GKeyFile *keyfile;
  gboolean retval;
  char *data = NULL;
  gsize len;

  g_return_val_if_fail (GTK_IS_PAGE_SETUP (page_setup), FALSE);
  g_return_val_if_fail (file_name != NULL, FALSE);

  keyfile = g_key_file_new ();
  retval = ephy_print_utils_page_setup_to_key_file (page_setup, keyfile, error);
  if (!retval) goto out;

  data = g_key_file_to_data (keyfile, &len, error);
  if (!data) goto out;

  retval = g_file_set_contents (file_name, data, len, error);

out:
  g_key_file_free (keyfile);
  g_free (data);

  return retval;
}

/**
 * ephy_print_utils_page_setup_to_key_file:
 * @page_setup: a #GtkPageSetup
 * @key_file: the #GKeyFile to save the print page_setup to
 * @error:
 * 
 * This function adds the print page_setup from @page_setup to @key_file.
 * 
 * Return value: %TRUE on success
 *
 * Since: 2.10
 */
gboolean
ephy_print_utils_page_setup_to_key_file (GtkPageSetup  *page_setup,
					 GKeyFile          *key_file,
					 GError           **error)
{
  GtkPaperSize *paper_size;
  const char *name, *ppd_name, *display_name;
  char *orientation;

  g_return_val_if_fail (GTK_IS_PAGE_SETUP (page_setup), FALSE);
  g_return_val_if_fail (key_file != NULL, FALSE);

  paper_size = gtk_page_setup_get_paper_size (page_setup);
  g_assert (paper_size != NULL);

  name = gtk_paper_size_get_name (paper_size);
  display_name = gtk_paper_size_get_display_name (paper_size);
  ppd_name = gtk_paper_size_get_ppd_name (paper_size);

  if (ppd_name != NULL) {
    g_key_file_set_string (key_file, PAPER_SIZE_GROUP,
			   "PPDName", ppd_name);
  } else {
    g_key_file_set_string (key_file, PAPER_SIZE_GROUP,
			   "Name", name);
  }

  if (display_name) {
    g_key_file_set_string (key_file, PAPER_SIZE_GROUP,
			   "DisplayName", display_name);
  }

  g_key_file_set_double (key_file, PAPER_SIZE_GROUP,
			 "Width", gtk_paper_size_get_width (paper_size, GTK_UNIT_MM));
  g_key_file_set_double (key_file, PAPER_SIZE_GROUP,
			 "Height", gtk_paper_size_get_height (paper_size, GTK_UNIT_MM));

  g_key_file_set_double (key_file, PAGE_SETUP_GROUP,
			 "MarginTop", gtk_page_setup_get_top_margin (page_setup, GTK_UNIT_MM));
  g_key_file_set_double (key_file, PAGE_SETUP_GROUP,
			 "MarginBottom", gtk_page_setup_get_bottom_margin (page_setup, GTK_UNIT_MM));
  g_key_file_set_double (key_file, PAGE_SETUP_GROUP,
			 "MarginLeft", gtk_page_setup_get_left_margin (page_setup, GTK_UNIT_MM));
  g_key_file_set_double (key_file, PAGE_SETUP_GROUP,
			 "MarginRight", gtk_page_setup_get_right_margin (page_setup, GTK_UNIT_MM));

  orientation = ephy_string_enum_to_string (GTK_TYPE_PAGE_ORIENTATION,
					    gtk_page_setup_get_orientation (page_setup));
  g_key_file_set_string (key_file, PAGE_SETUP_GROUP,
			 "Orientation", orientation);
  g_free (orientation);

  return TRUE;
}
