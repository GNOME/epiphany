/*
 *  Copyright (C) 2002 Jorn Baayen
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

#include "print-dialog.h"

#include "ephy-command-manager.h"
#include "ephy-prefs.h"

#include <gtk/gtktogglebutton.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkstock.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#define CONF_PRINT_BOTTOM_MARGIN "/apps/epiphany/dialogs/print_bottom_margin"
#define CONF_PRINT_TOP_MARGIN "/apps/epiphany/dialogs/print_top_margin"
#define CONF_PRINT_LEFT_MARGIN "/apps/epiphany/dialogs/print_left_margin"
#define CONF_PRINT_RIGHT_MARGIN "/apps/epiphany/dialogs/print_right_margin"
#define CONF_PRINT_PAGE_TITLE "/apps/epiphany/dialogs/print_page_title"
#define CONF_PRINT_PAGE_URL "/apps/epiphany/dialogs/print_page_url"
#define CONF_PRINT_DATE "/apps/epiphany/dialogs/print_date"
#define CONF_PRINT_PAGE_NUMBERS "/apps/epiphany/dialogs/print_page_numbers"
#define CONF_PRINT_PRINTER "/apps/epiphany/dialogs/print_printer"
#define CONF_PRINT_FILE "/apps/epiphany/dialogs/print_file"
#define CONF_PRINT_PRINTON "/apps/epiphany/dialogs/print_on"
#define CONF_PRINT_PAPER "/apps/epiphany/dialogs/print_paper"
#define CONF_PRINT_ALL_PAGES "/apps/epiphany/dialogs/print_all_pages"
#define CONF_PRINT_COLOR "/apps/epiphany/dialogs/print_color"
#define CONF_PRINT_ORIENTATION "/apps/epiphany/dialogs/print_orientation"

static void print_dialog_class_init (PrintDialogClass *klass);
static void print_dialog_init (PrintDialog *dialog);

/* Glade callbacks */
void
print_cancel_button_cb (GtkWidget *widget,
			EphyDialog *dialog);
void
print_ok_button_cb (GtkWidget *widget,
		    EphyDialog *dialog);
void
print_preview_button_cb (GtkWidget *widget,
	 	         EphyDialog *dialog);

static GObjectClass *parent_class = NULL;

#define EPHY_PRINT_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_PRINT_DIALOG, PrintDialogPrivate))

struct PrintDialogPrivate
{
	GtkWidget *window;
};

enum
{
	WINDOW_PROP,
	PRINTON_PROP,
	PRINTER_PROP,
	FILE_PROP,
	PAPER_PROP,
	TOP_PROP,
	BOTTOM_PROP,
	LEFT_PROP,
	RIGHT_PROP,
	PAGE_TITLE_PROP,
	PAGE_URL_PROP,
	PAGE_NUMBERS_PROP,
	DATE_PROP,
	ALL_PAGES_PROP,
	TO_PROP,
	FROM_PROP,
	COLOR_PROP,
	ORIENTATION_PROP,
	PREVIEW_PROP,
	SELECTION_PROP
};

enum
{
	PREVIEW,
	LAST_SIGNAL
};

static const
EphyDialogProperty properties [] =
{
	{ "print_dialog",			NULL,			  PT_NORMAL, 0 },
	{ "printer_radiobutton",		CONF_PRINT_PRINTON,	  PT_NORMAL, 0 },
	{ "printer_entry",			CONF_PRINT_PRINTER,	  PT_NORMAL, 0 },
	{ "file_entry",				CONF_PRINT_FILE,	  PT_NORMAL, 0 },
	{ "A4_radiobutton",			CONF_PRINT_PAPER,	  PT_NORMAL, G_TYPE_STRING },
	{ "top_spinbutton",			CONF_PRINT_TOP_MARGIN,	  PT_NORMAL, 0 },
        { "bottom_spinbutton",			CONF_PRINT_BOTTOM_MARGIN, PT_NORMAL, 0 },
	{ "left_spinbutton",			CONF_PRINT_LEFT_MARGIN,	  PT_NORMAL, 0 },
	{ "right_spinbutton",			CONF_PRINT_RIGHT_MARGIN,  PT_NORMAL, 0 },
	{ "print_page_title_checkbutton",	CONF_PRINT_PAGE_TITLE,	  PT_NORMAL, 0 },
	{ "print_page_url_checkbutton",		CONF_PRINT_PAGE_URL,	  PT_NORMAL, 0 },
	{ "print_page_numbers_checkbutton",	CONF_PRINT_PAGE_NUMBERS,  PT_NORMAL, 0 },
	{ "print_date_checkbutton",		CONF_PRINT_DATE,	  PT_NORMAL, 0 },
	{ "all_pages_radiobutton",		CONF_PRINT_ALL_PAGES,	  PT_NORMAL, 0 },
	{ "to_spinbutton",			NULL,			  PT_NORMAL, 0 },
	{ "from_spinbutton",			NULL,			  PT_NORMAL, 0 },
	{ "print_color_radiobutton",		CONF_PRINT_COLOR,	  PT_NORMAL, 0 },
	{ "orient_p_radiobutton",		CONF_PRINT_ORIENTATION,	  PT_NORMAL, 0 },
	{ "preview_button",			NULL,			  PT_NORMAL, 0 },
	{ "selection_radiobutton",		NULL,			  PT_NORMAL, 0 },

	{ NULL }
};

static const
char *paper_format_enum [] =
{
	"A4", "Letter", "Legal", "Executive"
};
static guint n_paper_format_enum = G_N_ELEMENTS (paper_format_enum);

static guint print_dialog_signals[LAST_SIGNAL] = { 0 };

GType
print_dialog_get_type (void)
{
        static GType print_dialog_type = 0;

        if (print_dialog_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (PrintDialogClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) print_dialog_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (PrintDialog),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) print_dialog_init
                };

                print_dialog_type = g_type_register_static (EPHY_TYPE_EMBED_DIALOG,
						            "PrintDialog",
						            &our_info, 0);
        }

        return print_dialog_type;

}

static void
impl_show (EphyDialog *dialog)
{
	PrintDialog *print_dialog = EPHY_PRINT_DIALOG (dialog);
	EphyEmbed *embed = ephy_embed_dialog_get_embed (EPHY_EMBED_DIALOG (dialog));

	EPHY_DIALOG_CLASS (parent_class)->show (dialog);

	if (print_dialog->only_collect_info)
	{
		GtkWidget *button;

		/* disappear preview button  */
		button = ephy_dialog_get_control (EPHY_DIALOG (dialog),
						  properties[PREVIEW_PROP].id);
		gtk_widget_hide (button);
	}

	if (ephy_command_manager_can_do_command
	    (EPHY_COMMAND_MANAGER (embed), "cmd_copy") == FALSE)
	{
		GtkWidget *widget;

		/* Make selection button disabled */
		widget = ephy_dialog_get_control (EPHY_DIALOG (dialog),
						  properties[SELECTION_PROP].id);

		gtk_widget_set_sensitive (widget, FALSE);

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
		{
			GtkWidget *all_pages;
			all_pages = ephy_dialog_get_control (EPHY_DIALOG (dialog),
							     properties[ALL_PAGES_PROP].id);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (all_pages), TRUE);
		}
	}
}

static void
print_dialog_class_init (PrintDialogClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
	EphyDialogClass *dialog_class = EPHY_DIALOG_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

	dialog_class->show = impl_show;

	print_dialog_signals[PREVIEW] =
                g_signal_new ("preview",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (PrintDialogClass, preview),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);

	g_type_class_add_private (object_class, sizeof(PrintDialogPrivate));
}

static void
print_dialog_init (PrintDialog *dialog)
{
	GdkPixbuf *icon;
	dialog->priv = EPHY_PRINT_DIALOG_GET_PRIVATE (dialog);

	dialog->only_collect_info = FALSE;
	dialog->ret_info = NULL;

	ephy_dialog_construct (EPHY_DIALOG(dialog),
				 properties,
				 "print.glade", "print_dialog");

	dialog->priv->window = ephy_dialog_get_control (EPHY_DIALOG(dialog),
							properties[WINDOW_PROP].id);

	ephy_dialog_add_enum (EPHY_DIALOG (dialog), properties[PAPER_PROP].id,
			      n_paper_format_enum, paper_format_enum);
	
	icon = gtk_widget_render_icon (dialog->priv->window, 
						      GTK_STOCK_PRINT,
						      GTK_ICON_SIZE_MENU,
						      "print_dialog");
	gtk_window_set_icon (GTK_WINDOW(dialog->priv->window), icon);
	g_object_unref (icon);
}

EphyDialog *
print_dialog_new (EphyEmbed *embed,
		  EmbedPrintInfo **ret_info)
{
	PrintDialog *dialog;

	dialog = EPHY_PRINT_DIALOG (g_object_new (EPHY_TYPE_PRINT_DIALOG,
						  "embed", embed,
						  NULL));

	if (ret_info != NULL)
	{
		dialog->only_collect_info = TRUE;
		dialog->ret_info = ret_info;
	}

	return EPHY_DIALOG(dialog);
}

EphyDialog *
print_dialog_new_with_parent (GtkWidget *window,
			      EphyEmbed *embed,
			      EmbedPrintInfo **ret_info)
{
	PrintDialog *dialog;

	dialog = EPHY_PRINT_DIALOG (g_object_new (EPHY_TYPE_PRINT_DIALOG,
						  "embed", embed,
						  "parent-window", window,
						  NULL));

	if (ret_info != NULL)
	{
		dialog->only_collect_info = TRUE;
		dialog->ret_info = ret_info;
	}

	return EPHY_DIALOG(dialog);
}

void
print_free_info (EmbedPrintInfo *info)
{
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

static EmbedPrintInfo *
print_get_info (EphyDialog *dialog)
{
	EmbedPrintInfo *info;
	GValue print_to_file = {0, };
	GValue printer = {0, };
	GValue file = {0, };
	GValue top_margin = {0, };
	GValue bottom_margin = {0, };
	GValue left_margin = {0, };
	GValue right_margin = {0, };
	GValue from_page = {0, };
	GValue to_page = {0, };
	GValue paper = {0, };
	GValue pages = {0, };
	GValue print_color = {0, };
	GValue orientation = {0, };
	GValue page_title = {0, };
	GValue page_url = {0, };
	GValue date = {0, };
	GValue page_numbers = {0, };
	const char *filename;

	info = g_new0 (EmbedPrintInfo, 1);

	ephy_dialog_get_value (dialog, properties[PRINTON_PROP].id, &print_to_file);
	info->print_to_file = g_value_get_int (&print_to_file);
	g_value_unset (&print_to_file);

	ephy_dialog_get_value (dialog, properties[PRINTER_PROP].id, &printer);
	info->printer = g_strdup (g_value_get_string (&printer));
	g_value_unset (&printer);

	ephy_dialog_get_value (dialog, properties[FILE_PROP].id, &file);
	filename = g_value_get_string (&file);
	if (filename != NULL)
	{
		info->file = gnome_vfs_expand_initial_tilde (g_value_get_string (&file));
	}
	else
	{
		info->file = NULL;
	}
	g_value_unset (&file);

	ephy_dialog_get_value (dialog, properties[BOTTOM_PROP].id, &bottom_margin);
	info->bottom_margin = g_value_get_float (&bottom_margin);
	g_value_unset (&bottom_margin);

	ephy_dialog_get_value (dialog, properties[LEFT_PROP].id, &left_margin);
	info->left_margin = g_value_get_float (&left_margin);
	g_value_unset (&left_margin);

	ephy_dialog_get_value (dialog, properties[TOP_PROP].id, &top_margin);
	info->top_margin = g_value_get_float (&top_margin);
	g_value_unset (&top_margin);

	ephy_dialog_get_value (dialog, properties[RIGHT_PROP].id, &right_margin);
	info->right_margin = g_value_get_float (&right_margin);
	g_value_unset (&right_margin);

	ephy_dialog_get_value (dialog, properties[FROM_PROP].id, &from_page);
	info->from_page = g_value_get_float (&from_page);
	g_value_unset (&from_page);

	ephy_dialog_get_value (dialog, properties[TO_PROP].id, &to_page);
	info->to_page = g_value_get_float (&to_page);
	g_value_unset (&to_page);

	ephy_dialog_get_value (dialog, properties[PAPER_PROP].id, &paper);
	info->paper = g_strdup (paper_format_enum[g_value_get_int (&paper)]);
	g_value_unset (&paper);

	ephy_dialog_get_value (dialog, properties[ALL_PAGES_PROP].id, &pages);
	info->pages = g_value_get_int (&pages);
	g_value_unset (&pages);

	ephy_dialog_get_value (dialog, properties[COLOR_PROP].id, &print_color);
	info->print_color = !g_value_get_int (&print_color);
	g_value_unset (&print_color);

	ephy_dialog_get_value (dialog, properties[ORIENTATION_PROP].id, &orientation);
	info->orientation = g_value_get_int (&orientation);
	g_value_unset (&orientation);

	info->frame_type = 0;

	ephy_dialog_get_value (dialog, properties[PAGE_TITLE_PROP].id, &page_title);
	info->header_left_string = g_value_get_boolean (&page_title) ?
				   g_strdup ("&T") : g_strdup ("");
	g_value_unset (&page_title);

	ephy_dialog_get_value (dialog, properties[PAGE_URL_PROP].id, &page_url);
	info->header_right_string = g_value_get_boolean (&page_url) ?
				    g_strdup ("&U") : g_strdup ("");
	g_value_unset (&page_url);

	ephy_dialog_get_value (dialog, properties[PAGE_NUMBERS_PROP].id, &page_numbers);
	info->footer_left_string = g_value_get_boolean (&page_numbers) ?
				   g_strdup ("&PT") : g_strdup ("");
	g_value_unset (&page_numbers);

	ephy_dialog_get_value (dialog, properties[DATE_PROP].id, &date);
	info->footer_right_string = g_value_get_boolean (&date) ?
				    g_strdup ("&D") : g_strdup ("");
	g_value_unset (&date);

	info->header_center_string = g_strdup("");
	info->footer_center_string = g_strdup("");

	return info;
}

static void
print_dialog_print (EphyDialog *dialog)
{
	EmbedPrintInfo *info;
	EphyEmbed *embed;

	info = print_get_info (dialog);

	if(EPHY_PRINT_DIALOG(dialog)->only_collect_info && EPHY_PRINT_DIALOG(dialog)->ret_info)
	{
		*(EPHY_PRINT_DIALOG(dialog)->ret_info) = info;

		/* When in collect_info mode the caller owns the reference */
		return;
	}

	embed = ephy_embed_dialog_get_embed
		(EPHY_EMBED_DIALOG(dialog));
	g_return_if_fail (embed != NULL);

	info->preview = FALSE;
	ephy_embed_print (embed, info);
	print_free_info (info);

	g_object_unref (G_OBJECT(dialog));
}

static void
print_dialog_preview (EphyDialog *dialog)
{
	EmbedPrintInfo *info;
	EphyEmbed *embed;

	/* Should not be called in collect_info mode */
	if(EPHY_PRINT_DIALOG(dialog)->only_collect_info && EPHY_PRINT_DIALOG(dialog)->ret_info)
	{
		g_return_if_reached ();
	}

	info = print_get_info (dialog);

	embed = ephy_embed_dialog_get_embed
		(EPHY_EMBED_DIALOG(dialog));
	g_return_if_fail (embed != NULL);

	info->preview = TRUE;
	ephy_embed_print (embed, info);
	print_free_info (info);

	g_signal_emit (G_OBJECT (dialog), print_dialog_signals[PREVIEW], 0);

	g_object_unref (G_OBJECT(dialog));
}

void
print_cancel_button_cb (GtkWidget *widget,
			EphyDialog *dialog)
{
	if (EPHY_PRINT_DIALOG (dialog)->only_collect_info)
	{
		/* When in collect_info mode the caller owns the reference */
		return;
	}

	g_object_unref (G_OBJECT(dialog));
}

void
print_ok_button_cb (GtkWidget *widget,
		    EphyDialog *dialog)
{
	print_dialog_print (dialog);
}

void
print_preview_button_cb (GtkWidget *widget,
		         EphyDialog *dialog)
{
	print_dialog_preview (dialog);
}
