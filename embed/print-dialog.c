/*
 *  Copyright (C) 2002 Jorn Baayen
 *  Copyright (C) 2003, 2004 Christian Persch
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

#include "print-dialog.h"
#include "ephy-embed-single.h"
#include "ephy-embed-shell.h"
#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "ephy-stock-icons.h"
#include "eel-gconf-extensions.h"
#include "ephy-debug.h"
#include "ephy-gui.h"

#include <gtk/gtkwindow.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcelllayout.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <glib/gi18n.h>

#define CONF_PRINT_PRINTER	"/apps/epiphany/dialogs/print_printer_name"
#define CONF_PRINT_FILE		"/apps/epiphany/dialogs/print_file"
#define CONF_PRINT_DIR		"/apps/epiphany/directories/print_to_file"
#define CONF_PRINT_PRINTON	"/apps/epiphany/dialogs/print_on"
#define CONF_PRINT_ALL_PAGES	"/apps/epiphany/dialogs/print_all_pages"
#define CONF_PRINT_FROM_PAGE	"/apps/epiphany/dialogs/print_from_page"
#define CONF_PRINT_TO_PAGE	"/apps/epiphany/dialogs/print_to_page"

#define CONF_PRINT_BOTTOM_MARGIN	"/apps/epiphany/dialogs/print_bottom_margin"
#define CONF_PRINT_TOP_MARGIN		"/apps/epiphany/dialogs/print_top_margin"
#define CONF_PRINT_LEFT_MARGIN		"/apps/epiphany/dialogs/print_left_margin"
#define CONF_PRINT_RIGHT_MARGIN		"/apps/epiphany/dialogs/print_right_margin"
#define CONF_PRINT_PAGE_TITLE		"/apps/epiphany/dialogs/print_page_title"
#define CONF_PRINT_PAGE_URL		"/apps/epiphany/dialogs/print_page_url"
#define CONF_PRINT_DATE			"/apps/epiphany/dialogs/print_date"
#define CONF_PRINT_PAGE_NUMBERS		"/apps/epiphany/dialogs/print_page_numbers"
#define CONF_PRINT_PAPER		"/apps/epiphany/dialogs/print_paper"
#define CONF_PRINT_COLOR		"/apps/epiphany/dialogs/print_color"
#define CONF_PRINT_ORIENTATION		"/apps/epiphany/dialogs/print_orientation"

enum
{
	WINDOW_PROP,
	PRINTON_PROP,
	PRINTER_PROP,
	FILE_PROP,
	BROWSE_PROP,
	ALL_PAGES_PROP,
	SELECTION_PROP,
	TO_PROP,
	FROM_PROP
};

enum
{
	COL_PRINTER_DISPLAY_NAME,
	COL_PRINTER_NAME
};

static const
EphyDialogProperty print_props [] =
{
	{ "print_dialog",			NULL,			  PT_NORMAL,    0 },
	{ "printer_radiobutton",		CONF_PRINT_PRINTON,	  PT_AUTOAPPLY, 0 },
	{ "printer_combobox",			CONF_PRINT_PRINTER,	  PT_AUTOAPPLY, G_TYPE_STRING },
	{ "file_entry",				CONF_PRINT_FILE,	  PT_AUTOAPPLY, 0 },
	{ "browse_button",			NULL,			  PT_NORMAL,	0 },
	{ "all_pages_radiobutton",		CONF_PRINT_ALL_PAGES,	  PT_AUTOAPPLY, 0 },
	{ "selection_radiobutton",		NULL,			  PT_NORMAL,    0 },
	{ "to_spinbutton",			CONF_PRINT_FROM_PAGE,	  PT_AUTOAPPLY, G_TYPE_INT },
	{ "from_spinbutton",			CONF_PRINT_TO_PAGE,	  PT_AUTOAPPLY, G_TYPE_INT },

	{ NULL }
};

enum
{
	SETUP_WINDOW_PROP,
	PAPER_PROP,
	TOP_PROP,
	BOTTOM_PROP,
	LEFT_PROP,
	RIGHT_PROP,
	PAGE_TITLE_PROP,
	PAGE_URL_PROP,
	PAGE_NUMBERS_PROP,
	DATE_PROP,
	COLOR_PROP,
	ORIENTATION_PROP,
};

static const
EphyDialogProperty setup_props [] =
{
	{ "print_setup_dialog",			NULL,			  PT_NORMAL,    0 },
	{ "A4_radiobutton",			CONF_PRINT_PAPER,	  PT_AUTOAPPLY, G_TYPE_STRING },
	{ "top_spinbutton",			CONF_PRINT_TOP_MARGIN,	  PT_AUTOAPPLY, G_TYPE_INT },
	{ "bottom_spinbutton",			CONF_PRINT_BOTTOM_MARGIN, PT_AUTOAPPLY, G_TYPE_INT },
	{ "left_spinbutton",			CONF_PRINT_LEFT_MARGIN,	  PT_AUTOAPPLY, G_TYPE_INT },
	{ "right_spinbutton",			CONF_PRINT_RIGHT_MARGIN,  PT_AUTOAPPLY, G_TYPE_INT },
	{ "print_page_title_checkbutton",	CONF_PRINT_PAGE_TITLE,	  PT_AUTOAPPLY, 0 },
	{ "print_page_url_checkbutton",		CONF_PRINT_PAGE_URL,	  PT_AUTOAPPLY, 0 },
	{ "print_page_numbers_checkbutton",	CONF_PRINT_PAGE_NUMBERS,  PT_AUTOAPPLY, 0 },
	{ "print_date_checkbutton",		CONF_PRINT_DATE,	  PT_AUTOAPPLY, 0 },
	{ "print_color_radiobutton",		CONF_PRINT_COLOR,	  PT_AUTOAPPLY, 0 },
	{ "orient_p_radiobutton",		CONF_PRINT_ORIENTATION,	  PT_AUTOAPPLY, 0 },

	{ NULL }
};

static const
char *paper_format_enum [] =
{
	"A4", "Letter", "Legal", "Executive"
};
static guint n_paper_format_enum = G_N_ELEMENTS (paper_format_enum);

void ephy_print_dialog_response_cb		(GtkWidget *widget,
						 int response,
						 EphyDialog *dialog);
void ephy_print_dialog_browse_button_cb		(GtkWidget *widget,
						 EphyDialog *dialog);
void ephy_print_setup_dialog_close_button_cb	(GtkWidget *widget,
						 EphyDialog *dialog);
void ephy_print_setup_dialog_help_button_cb	(GtkWidget *widget,
						 EphyDialog *dialog);

void
ephy_print_info_free (EmbedPrintInfo *info)
{
	g_return_if_fail (info != NULL);

	g_free (info->printer);
	g_free (info->file);
	g_free (info->paper);
	g_free (info->header_left_string);
	g_free (info->header_center_string);
	g_free (info->header_right_string);
	g_free (info->footer_left_string);
	g_free (info->footer_center_string);
	g_free (info->footer_right_string);
	g_free (info);
}

static char *
sanitize_filename (const char *input)
{
	char *dir, *filename;

	if (input == NULL) return NULL;

	if (g_path_is_absolute (input) == FALSE)
	{
		dir = eel_gconf_get_string (CONF_PRINT_DIR);
		/* Fallback */
		if (dir == NULL || g_path_is_absolute (dir) == FALSE)
		{
			g_free (dir);
			dir = g_get_current_dir ();
		}
		/* Fallback */
		if (dir == NULL)
		{
			dir = g_strdup (g_get_home_dir ());
		}

		filename = g_build_filename (dir, input, NULL);
		g_free (dir);
	}
	else
	{
		filename = g_strdup (input);
	}

	dir = g_path_get_dirname (filename);
	if (dir == NULL || g_file_test (dir, G_FILE_TEST_IS_DIR) == FALSE)
	{
		g_free (filename);
		filename = NULL;
	}
	g_free (dir);

	return filename;
}

EmbedPrintInfo *
ephy_print_get_print_info (void)
{
	EmbedPrintInfo *info;
	char *filename, *converted, *expanded, *fname = NULL;

	info = g_new0 (EmbedPrintInfo, 1);

	info->print_to_file = eel_gconf_get_integer (print_props[PRINTON_PROP].pref) == 1;

	if (info->print_to_file)
	{
		filename = eel_gconf_get_string (print_props[FILE_PROP].pref);
		if (filename != NULL)
		{
			converted = g_filename_from_utf8 (filename, -1, NULL, NULL, NULL);
			if (converted != NULL)
			{
				expanded = gnome_vfs_expand_initial_tilde (filename);
				fname = sanitize_filename (expanded);
				g_free (expanded);
				g_free (converted);
			}
		}
		g_free (filename);

		/* fallback */
		if (fname == NULL)
		{
			fname = sanitize_filename ("output.ps");
		}

		info->file = g_filename_to_utf8 (fname, -1, NULL, NULL, NULL);
		g_free (fname);
	}

	info->printer = eel_gconf_get_string (print_props[PRINTER_PROP].pref);

	info->pages = eel_gconf_get_integer (print_props[ALL_PAGES_PROP].pref);
	info->from_page = eel_gconf_get_integer (print_props[FROM_PROP].pref);
	info->to_page = eel_gconf_get_integer (print_props[TO_PROP].pref);

	info->paper = eel_gconf_get_string (setup_props[PAPER_PROP].pref);
	info->orientation = eel_gconf_get_integer (setup_props[ORIENTATION_PROP].pref);
	info->print_color = ! eel_gconf_get_integer (setup_props[COLOR_PROP].pref);

	info->bottom_margin = eel_gconf_get_integer (setup_props[BOTTOM_PROP].pref);
	info->top_margin = eel_gconf_get_integer (setup_props[TOP_PROP].pref);
	info->left_margin = eel_gconf_get_integer (setup_props[LEFT_PROP].pref);
	info->right_margin = eel_gconf_get_integer (setup_props[RIGHT_PROP].pref);

	info->header_left_string = eel_gconf_get_boolean (setup_props[PAGE_TITLE_PROP].pref) ?
				   g_strdup ("&T") : g_strdup ("");
	info->header_right_string = eel_gconf_get_boolean (setup_props[PAGE_URL_PROP].pref) ?
				    g_strdup ("&U") : g_strdup ("");
	info->footer_left_string = eel_gconf_get_boolean (setup_props[PAGE_NUMBERS_PROP].pref) ?
				   g_strdup ("&PT") : g_strdup ("");
	info->footer_right_string = eel_gconf_get_boolean (setup_props[DATE_PROP].pref) ?
				    g_strdup ("&D") : g_strdup ("");
	info->header_center_string = g_strdup("");
	info->footer_center_string = g_strdup("");

	info->frame_type = 0;

	return info;
}

void
ephy_print_dialog_response_cb (GtkWidget *widget,
			       int response,
			       EphyDialog *dialog)
{
	switch (response)
	{
		case GTK_RESPONSE_HELP:
			ephy_gui_help (GTK_WINDOW (widget), "epiphany", "to-print-page");
			return;
		default:
			break;
	}
}

static void
print_filechooser_response_cb (GtkDialog *fc,
			       int response,
			       EphyDialog *dialog)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		char *filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		if (filename != NULL)
		{
			GtkWidget *entry;
			char *converted;

			converted = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);

			entry = ephy_dialog_get_control (dialog, print_props[FILE_PROP].id);
			gtk_entry_set_text (GTK_ENTRY (entry), converted);

			g_free (converted);
			g_free (filename);
		}
	}

	gtk_widget_destroy (GTK_WIDGET (fc));
}

void
ephy_print_dialog_browse_button_cb (GtkWidget *widget,
				    EphyDialog *dialog)
{
	GtkWidget *parent;
	EphyFileChooser *fc;
	GtkFileFilter *filter;

	parent = ephy_dialog_get_control (dialog, print_props[WINDOW_PROP].id);

	fc = ephy_file_chooser_new (_("Print to"),
				    GTK_WIDGET (parent),
				    GTK_FILE_CHOOSER_ACTION_SAVE,
				    CONF_PRINT_DIR, EPHY_FILE_FILTER_NONE);

	filter = ephy_file_chooser_add_mime_filter (fc, _("Postscript files"),
					   "application/postscript", NULL);

	ephy_file_chooser_add_pattern_filter (fc, _("All files"), "*", NULL);

	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (fc), filter);

	g_signal_connect (GTK_DIALOG (fc), "response",
			  G_CALLBACK (print_filechooser_response_cb),
			  dialog);

	gtk_window_set_modal (GTK_WINDOW (fc), TRUE);
	gtk_widget_show (GTK_WIDGET (fc));
}

void
ephy_print_setup_dialog_close_button_cb (GtkWidget *widget,
					 EphyDialog *dialog)
{
	g_object_unref (dialog);
}

void
ephy_print_setup_dialog_help_button_cb (GtkWidget *widget,
					 EphyDialog *dialog)
{
	ephy_gui_help (GTK_WINDOW (dialog), "epiphany", "using-print-setup");
}

EphyDialog *
ephy_print_dialog_new (GtkWidget *parent,
		       EphyEmbed *embed)
{
	EphyDialog *dialog;
	GtkWidget *widget;
	GList *printers, *l;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkCellRenderer *renderer;
	EphyEmbedSingle *single;

	dialog =  ephy_dialog_new_with_parent (parent);

	ephy_dialog_construct (dialog, 
			       print_props,
			       ephy_file ("print.glade"),
			       "print_dialog",
			       NULL);

	widget = ephy_dialog_get_control (dialog, print_props[WINDOW_PROP].id);
	gtk_window_set_icon_name (GTK_WINDOW (widget), GTK_STOCK_PRINT);

	widget = ephy_dialog_get_control (dialog, print_props[BROWSE_PROP].id);
	gtk_widget_set_sensitive (widget, eel_gconf_key_is_writable (CONF_PRINT_FILE));

	widget = ephy_dialog_get_control (dialog, print_props[PRINTER_PROP].id);
	single = EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (embed_shell));
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	printers = ephy_embed_single_get_printer_list (single);
	for (l = printers; l != NULL; l = l->next)
	{
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_PRINTER_DISPLAY_NAME, l->data,
				    COL_PRINTER_NAME, l->data,
				    -1);
	}
	g_list_foreach (printers, (GFunc)g_free, NULL);
	g_list_free (printers);

	gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (store));
	g_object_unref (store);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (widget), renderer,
					"text", COL_PRINTER_DISPLAY_NAME,
					NULL);
	ephy_dialog_set_data_column (dialog, print_props[PRINTER_PROP].id,
				     COL_PRINTER_NAME);

	return dialog;
}

EphyDialog *
ephy_print_setup_dialog_new (void)
{
	EphyDialog *dialog;
	GtkWidget *window;

	dialog = EPHY_DIALOG (g_object_new (EPHY_TYPE_DIALOG, NULL));

	ephy_dialog_construct (dialog,
			       setup_props,
			       ephy_file ("print.glade"),
			       "print_setup_dialog",
			       NULL);

	ephy_dialog_add_enum (dialog, setup_props[PAPER_PROP].id,
			      n_paper_format_enum, paper_format_enum);

	window = ephy_dialog_get_control (dialog, setup_props[SETUP_WINDOW_PROP].id);
	gtk_window_set_icon_name (GTK_WINDOW (window), STOCK_PRINT_SETUP);

	return dialog;
}
