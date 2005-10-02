/*
 *  Copyright (C) 2002 Jorn Baayen
 *  Copyright (C) 2003, 2004 Christian Persch
 *  Copyright (C) 2005 Juerg Billeter
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

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcelllayout.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <glib/gi18n.h>

#include <libgnomeprintui/gnome-print-paper-selector.h>

#define CONF_PRINT_PAGE_TITLE		"/apps/epiphany/dialogs/print_page_title"
#define CONF_PRINT_PAGE_URL		"/apps/epiphany/dialogs/print_page_url"
#define CONF_PRINT_DATE			"/apps/epiphany/dialogs/print_date"
#define CONF_PRINT_PAGE_NUMBERS		"/apps/epiphany/dialogs/print_page_numbers"
#define CONF_PRINT_COLOR		"/apps/epiphany/dialogs/print_color"

#define PRINT_CONFIG_FILENAME "ephy-print-config.xml"

enum
{
	SETUP_WINDOW_PROP,
	PAGE_TITLE_PROP,
	PAGE_URL_PROP,
	PAGE_NUMBERS_PROP,
	DATE_PROP,
	COLOR_PROP,
	PAPER_SELECTOR_PROP,
};

static const
EphyDialogProperty setup_props [] =
{
	{ "print_setup_dialog",			NULL,			  PT_NORMAL,    0 },
	{ "print_page_title_checkbutton",	CONF_PRINT_PAGE_TITLE,	  PT_AUTOAPPLY, 0 },
	{ "print_page_url_checkbutton",		CONF_PRINT_PAGE_URL,	  PT_AUTOAPPLY, 0 },
	{ "print_page_numbers_checkbutton",	CONF_PRINT_PAGE_NUMBERS,  PT_AUTOAPPLY, 0 },
	{ "print_date_checkbutton",		CONF_PRINT_DATE,	  PT_AUTOAPPLY, 0 },
	{ "print_color_radiobutton",		CONF_PRINT_COLOR,	  PT_AUTOAPPLY, 0 },
	{ "print_paper_selector_hbox",		NULL,			  PT_NORMAL,    0 },

	{ NULL }
};

void ephy_print_dialog_response_cb		(GtkDialog *dialog,
						 int response,
						 EmbedPrintInfo *info);
void ephy_print_setup_dialog_close_button_cb	(GtkWidget *widget,
						 EphyDialog *dialog);
void ephy_print_setup_dialog_help_button_cb	(GtkWidget *widget,
						 EphyDialog *dialog);

void
ephy_print_info_free (EmbedPrintInfo *info)
{
	g_return_if_fail (info != NULL);
	
	g_object_unref (info->config);
	
	g_free (info->tempfile);
	
	if (info->cancel_print_id != 0)
		g_signal_handler_disconnect (embed_shell, info->cancel_print_id);

	g_free (info->header_left_string);
	g_free (info->header_center_string);
	g_free (info->header_right_string);
	g_free (info->footer_left_string);
	g_free (info->footer_center_string);
	g_free (info->footer_right_string);
	g_free (info);
}

static GnomePrintConfig *
ephy_print_load_config_from_file (void)
{
	GnomePrintConfig *ephy_print_config = NULL;
	char *file_name, *contents = NULL;

	file_name = g_build_filename (ephy_dot_dir (),
				      PRINT_CONFIG_FILENAME,
				      NULL);
	
	if (g_file_get_contents (file_name, &contents, NULL, NULL))
	{
		ephy_print_config = gnome_print_config_from_string (contents, 0);
		g_free (contents);
	}

	if (ephy_print_config == NULL)
	{
		ephy_print_config = gnome_print_config_default ();
	}

	g_free (file_name);
	
	return ephy_print_config;
}

static void
ephy_print_save_config_to_file (GnomePrintConfig *config)
{
	char *file_name, *str;

	g_return_if_fail (config != NULL);

	str = gnome_print_config_to_string (config, 0);
	if (str == NULL) return;

	file_name = g_build_filename (ephy_dot_dir (), PRINT_CONFIG_FILENAME,
				      NULL);
	
	g_file_set_contents (file_name, str, -1, NULL);

	g_free (file_name);
	g_free (str);
}

static void
ephy_print_save_config_to_file_and_unref (GnomePrintConfig *config)
{
	ephy_print_save_config_to_file (config);
	
	g_object_unref (G_OBJECT (config));
}

EmbedPrintInfo *
ephy_print_get_print_info (void)
{
	EmbedPrintInfo *info;

	info = g_new0 (EmbedPrintInfo, 1);
	
	info->config = ephy_print_load_config_from_file ();
	
	info->range = GNOME_PRINT_RANGE_ALL;
	info->from_page = 1;
	info->to_page = 1;

	info->print_color = ! eel_gconf_get_integer (setup_props[COLOR_PROP].pref);

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
ephy_print_dialog_response_cb (GtkDialog *dialog,
			       int response,
			       EmbedPrintInfo *info)
{
	if (response == GNOME_PRINT_DIALOG_RESPONSE_PRINT)
	{
		ephy_print_save_config_to_file (info->config);
		
		info->range = gnome_print_dialog_get_range_page (GNOME_PRINT_DIALOG (dialog),
								 &info->from_page,
								 &info->to_page);
	}
}

static gboolean
using_pdf_printer (GnomePrintConfig *config)
{
	const guchar *driver;

	driver = gnome_print_config_get (
		config, (const guchar *)"Settings.Engine.Backend.Driver");

	if (driver)
        {
		if (!strcmp ((const gchar *)driver, "gnome-print-pdf"))
			return TRUE;
		else
			return FALSE;
	}

	return FALSE;
}

static gboolean
using_postscript_printer (GnomePrintConfig *config)
{
	const guchar *driver;
	const guchar *transport;

	driver = gnome_print_config_get (
		config, (const guchar *)"Settings.Engine.Backend.Driver");

	transport = gnome_print_config_get (
		config, (const guchar *)"Settings.Transport.Backend");

	if (driver)
	{
		if (strcmp ((const gchar *)driver, "gnome-print-ps") == 0)
			return TRUE;
		else
			return FALSE;
	}
	else if (transport) /* these transports default to PostScript */
	{
		if (strcmp ((const gchar *)transport, "CUPS") == 0)
			return TRUE;
		else if (strcmp ((const gchar *)transport, "LPD") == 0)
			return TRUE;
	}

	return FALSE;
}

gboolean
ephy_print_verify_postscript (GnomePrintDialog *print_dialog)
{
	GnomePrintConfig *config;
	GtkWidget *dialog;
	
	config = gnome_print_dialog_get_config (print_dialog);

	if (using_postscript_printer (config))
		return TRUE;
	
        if (using_pdf_printer (config))
        {
                dialog = gtk_message_dialog_new (
                        GTK_WINDOW (print_dialog), GTK_DIALOG_MODAL,
                        GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                        _("Generating PDF is not supported"));
        }
        else
        {
                dialog = gtk_message_dialog_new (
                        GTK_WINDOW (print_dialog), GTK_DIALOG_MODAL,
                        GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                        _("Printing is not supported on this printer"));
                gtk_message_dialog_format_secondary_text (
                        GTK_MESSAGE_DIALOG (dialog),
                        _("You were trying to print to a printer using the \"%s\" driver. This program requires a PostScript printer driver."),
                        gnome_print_config_get (
                                config, (guchar *)"Settings.Engine.Backend.Driver"));
        }

	if (GTK_WINDOW (print_dialog)->group)
		gtk_window_group_add_window (GTK_WINDOW (print_dialog)->group,
					     GTK_WINDOW (dialog));

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	
	return FALSE;
}

static void
cancel_print_cb (EphyEmbedShell *shell, EmbedPrintInfo *info)
{
	g_source_remove (info->print_idle_id);
	
	ephy_print_info_free (info);
}

static gboolean
ephy_print_do_print_idle_cb (EmbedPrintInfo *info)
{
	GnomePrintJob *job;

	/* Sometimes mozilla doesn't even create the temp file!? */
	if (g_file_test (info->tempfile, G_FILE_TEST_EXISTS) == FALSE) return FALSE;

	/* FIXME: is this actually necessary? libc docs say all streams
	 * are flushed when reading from any stream.
	 */
	fflush(NULL);

	job = gnome_print_job_new (info->config);

	gnome_print_job_set_file (job, info->tempfile);
	gnome_print_job_print (job);
	g_object_unref (job);

	unlink (info->tempfile);

	ephy_print_info_free (info);

	return FALSE;
}

void
ephy_print_do_print_and_free (EmbedPrintInfo *info)
{
	/* mozilla printing system hasn't really sent the data
	 * to the printer when reporting that printing is done, it's
	 * just ready to do it now
	 */
	info->print_idle_id = g_idle_add ((GSourceFunc) ephy_print_do_print_idle_cb,
					  info);
	info->cancel_print_id = g_signal_connect (embed_shell, "prepare-close",
						  G_CALLBACK (cancel_print_cb),
						  info);
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

static GtkWidget *
ephy_print_paper_selector_new ()
{
	GtkWidget *paper_selector;
	GnomePrintConfig *config;
	
	config = ephy_print_load_config_from_file ();

	paper_selector = gnome_paper_selector_new_with_flags (config,
						GNOME_PAPER_SELECTOR_MARGINS);
	
	g_object_set_data_full (G_OBJECT (paper_selector), "config", config,
				(GDestroyNotify) ephy_print_save_config_to_file_and_unref);
	
	return paper_selector;
}

/*
 * A variant of gnome_print_dialog_construct_range_page that can be used when
 * the total page count is unknown. It defaults to 1-1
 */
static void
ephy_print_dialog_construct_range_page (GnomePrintDialog *gpd, gint flags,
					const guchar *currentlabel, const guchar *rangelabel)
{
	GtkWidget *hbox;

	hbox = NULL;

	if (flags & GNOME_PRINT_RANGE_RANGE) {
		GtkWidget *l, *sb;
		GtkObject *a;
		AtkObject *atko;

		hbox = gtk_hbox_new (FALSE, 3);
		gtk_widget_show (hbox);

		l = gtk_label_new_with_mnemonic (_("_From:"));
		gtk_widget_show (l);
		gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);

		a = gtk_adjustment_new (1, 1, 9999, 1, 10, 10);
		g_object_set_data (G_OBJECT (hbox), "from", a);
		sb = gtk_spin_button_new (GTK_ADJUSTMENT (a), 1, 0.0);
		gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (sb), TRUE);
		gtk_widget_show (sb);
		gtk_box_pack_start (GTK_BOX (hbox), sb, FALSE, FALSE, 0);
		gtk_label_set_mnemonic_widget ((GtkLabel *) l, sb);

		atko = gtk_widget_get_accessible (sb);
		atk_object_set_description (atko, _("Sets the start of the range of pages to be printed"));

		l = gtk_label_new_with_mnemonic (_("_To:"));
		gtk_widget_show (l);
		gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);

		a = gtk_adjustment_new (1, 1, 9999, 1, 10, 10);
		g_object_set_data (G_OBJECT (hbox), "to", a);
		sb = gtk_spin_button_new (GTK_ADJUSTMENT (a), 1, 0.0);
		gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (sb), TRUE);
		gtk_widget_show (sb);
		gtk_box_pack_start (GTK_BOX (hbox), sb, FALSE, FALSE, 0);
		gtk_label_set_mnemonic_widget ((GtkLabel *) l, sb);

		atko = gtk_widget_get_accessible (sb);
		atk_object_set_description (atko, _("Sets the end of the range of pages to be printed"));
	}

	gnome_print_dialog_construct_range_any (gpd, flags, hbox, currentlabel, rangelabel);
}

GtkWidget *
ephy_print_dialog_new (GtkWidget *parent,
		       EmbedPrintInfo *info)
{
	GtkWidget *dialog;
	
	dialog = g_object_new (GNOME_TYPE_PRINT_DIALOG, "print_config",
			       info->config, NULL);

	gnome_print_dialog_construct (GNOME_PRINT_DIALOG (dialog), (const guchar *) _("Print"),
				      GNOME_PRINT_DIALOG_RANGE |
				      GNOME_PRINT_DIALOG_COPIES);
	
	ephy_print_dialog_construct_range_page (GNOME_PRINT_DIALOG (dialog),
						GNOME_PRINT_RANGE_ALL |
						GNOME_PRINT_RANGE_RANGE |
						GNOME_PRINT_RANGE_SELECTION,
						NULL, (const guchar *) _("Pages"));
	
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
                                           GNOME_PRINT_DIALOG_RESPONSE_PREVIEW,
                                           FALSE);

	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (ephy_print_dialog_response_cb), info);
	
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));
	
	if (GTK_WINDOW (parent)->group)
		gtk_window_group_add_window (GTK_WINDOW (parent)->group,
					     GTK_WINDOW (dialog));

	return dialog;
}

EphyDialog *
ephy_print_setup_dialog_new (void)
{
	EphyDialog *dialog;
	GtkWidget *window;
	GtkWidget *paper_selector_hbox;

	dialog = EPHY_DIALOG (g_object_new (EPHY_TYPE_DIALOG, NULL));

	ephy_dialog_construct (dialog,
			       setup_props,
			       ephy_file ("print.glade"),
			       "print_setup_dialog",
			       NULL);

	window = ephy_dialog_get_control (dialog, setup_props[SETUP_WINDOW_PROP].id);
	gtk_window_set_icon_name (GTK_WINDOW (window), STOCK_PRINT_SETUP);
	
	paper_selector_hbox = ephy_dialog_get_control (dialog,
						setup_props[PAPER_SELECTOR_PROP].id);
	gtk_box_pack_start_defaults (GTK_BOX (paper_selector_hbox),
			   ephy_print_paper_selector_new ());
	gtk_widget_show_all (paper_selector_hbox);

	return dialog;
}
