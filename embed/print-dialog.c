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
 */

#include "print-dialog.h"
#include "ephy-prefs.h"
#include <gtk/gtkdialog.h>
#include <gtk/gtkstock.h>

#define CONF_PRINT_BOTTOM_MARGIN "/apps/epiphany/print/bottom_margin"
#define CONF_PRINT_TOP_MARGIN "/apps/epiphany/print/top_margin"
#define CONF_PRINT_LEFT_MARGIN "/apps/epiphany/print/left_margin"
#define CONF_PRINT_RIGHT_MARGIN "/apps/epiphany/print/right_margin"
#define CONF_PRINT_PAGE_TITLE "/apps/epiphany/print/page_title_toggle"
#define CONF_PRINT_PAGE_URL "/apps/epiphany/print/page_url_toggle"
#define CONF_PRINT_DATE "/apps/epiphany/print/date_toggle"
#define CONF_PRINT_PAGE_NUMBERS "/apps/epiphany/print/page_numbers_toggle"
#define CONF_PRINT_PRINTER "/apps/epiphany/print/printer"
#define CONF_PRINT_FILE "/apps/epiphany/print/file"
#define CONF_PRINT_PRINTON "/apps/epiphany/print/printon"
#define CONF_PRINT_PAPER "/apps/epiphany/print/paper"
#define CONF_PRINT_ALL_PAGES "/apps/epiphany/print/all_pages"
#define CONF_PRINT_START_FROM_LAST "/apps/epiphany/print/start_from_last"
#define CONF_PRINT_COLOR "/apps/epiphany/print/color"
#define CONF_PRINT_ORIENTATION "/apps/epiphany/print/orientation"

static void print_dialog_class_init (PrintDialogClass *klass);
static void print_dialog_init (PrintDialog *dialog);
static void print_dialog_finalize (GObject *object);

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

struct PrintDialogPrivate
{
	gpointer dummy;
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
	ORIENTATION_PROP
};

enum
{
	PREVIEW,
	LAST_SIGNAL
};

static const
EphyDialogProperty properties [] =
{
	{ WINDOW_PROP, "print_dialog", NULL, PT_NORMAL, NULL },
	{ PRINTON_PROP, "printer_radiobutton", CONF_PRINT_PRINTON, PT_NORMAL, NULL },
	{ PRINTER_PROP, "printer_entry", CONF_PRINT_PRINTER, PT_NORMAL, NULL },
	{ FILE_PROP, "file_entry", CONF_PRINT_FILE, PT_NORMAL, NULL },
	{ PAPER_PROP,"letter_radiobutton", CONF_PRINT_PAPER, PT_NORMAL, NULL },
	{ TOP_PROP, "top_spinbutton", CONF_PRINT_TOP_MARGIN, PT_NORMAL, NULL },
        { BOTTOM_PROP, "bottom_spinbutton", CONF_PRINT_BOTTOM_MARGIN, PT_NORMAL, NULL },
	{ LEFT_PROP,"left_spinbutton", CONF_PRINT_LEFT_MARGIN, PT_NORMAL, NULL },
	{ RIGHT_PROP, "right_spinbutton", CONF_PRINT_RIGHT_MARGIN, PT_NORMAL, NULL },
	{ PAGE_TITLE_PROP, "print_page_title_checkbutton", CONF_PRINT_PAGE_TITLE, PT_NORMAL, NULL },
	{ PAGE_URL_PROP, "print_page_url_checkbutton", CONF_PRINT_PAGE_URL, PT_NORMAL, NULL },
	{ PAGE_NUMBERS_PROP, "print_page_numbers_checkbutton", CONF_PRINT_PAGE_NUMBERS, PT_NORMAL, NULL },
	{ DATE_PROP, "print_date_checkbutton", CONF_PRINT_DATE, PT_NORMAL, NULL },
	{ ALL_PAGES_PROP, "all_pages_radiobutton", CONF_PRINT_ALL_PAGES, PT_NORMAL, NULL },
	{ TO_PROP, "to_spinbutton", NULL, PT_NORMAL, NULL },
	{ FROM_PROP, "from_spinbutton", NULL, PT_NORMAL, NULL },
	{ COLOR_PROP, "print_color_radiobutton", CONF_PRINT_COLOR, PT_NORMAL, NULL },
	{ ORIENTATION_PROP, "orient_p_radiobutton", CONF_PRINT_ORIENTATION, PT_NORMAL, NULL },

	{ -1, NULL, NULL }
};

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

                print_dialog_type = g_type_register_static (EPHY_EMBED_DIALOG_TYPE,
						            "PrintDialog",
						            &our_info, 0);
        }

        return print_dialog_type;

}

static void
print_dialog_class_init (PrintDialogClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = print_dialog_finalize;

	print_dialog_signals[PREVIEW] =
                g_signal_new ("preview",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (PrintDialogClass, preview),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);
}

static void
print_dialog_init (PrintDialog *dialog)
{
	GdkPixbuf *icon;
	dialog->priv = g_new0 (PrintDialogPrivate, 1);

	dialog->only_collect_info = FALSE;

	dialog->ret_info = NULL;

	ephy_dialog_construct (EPHY_DIALOG(dialog),
				 properties,
				 "print.glade", "print_dialog");

	dialog->priv->window = ephy_dialog_get_control (EPHY_DIALOG(dialog), WINDOW_PROP);
	
	icon = gtk_widget_render_icon (dialog->priv->window, 
						      GTK_STOCK_PRINT,
						      GTK_ICON_SIZE_MENU,
						      "print_dialog");
	gtk_window_set_icon (GTK_WINDOW(dialog->priv->window), icon);
	g_object_unref (icon);
}

static void
print_dialog_finalize (GObject *object)
{
	PrintDialog *dialog;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_PRINT_DIALOG (object));

	dialog = PRINT_DIALOG (object);

        g_return_if_fail (dialog->priv != NULL);

        g_free (dialog->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyDialog *
print_dialog_new (EphyEmbed *embed,
		  EmbedPrintInfo **ret_info)
{
	PrintDialog *dialog;

	dialog = PRINT_DIALOG (g_object_new (PRINT_DIALOG_TYPE,
				     "EphyEmbed", embed,
				     NULL));

	if (!embed) dialog->only_collect_info = TRUE;
	dialog->ret_info = ret_info;

	return EPHY_DIALOG(dialog);
}

EphyDialog *
print_dialog_new_with_parent (GtkWidget *window,
			      EphyEmbed *embed,
			      EmbedPrintInfo **ret_info)
{
	PrintDialog *dialog;

	dialog = PRINT_DIALOG (g_object_new (PRINT_DIALOG_TYPE,
				     "EphyEmbed", embed,
				     "ParentWindow", window,
				     NULL));

	if (!embed) dialog->only_collect_info = TRUE;
	dialog->ret_info = ret_info;

	return EPHY_DIALOG(dialog);
}

void
print_free_info (EmbedPrintInfo *info)
{
	g_free (info->printer);
	g_free (info->file);
	g_free (info->header_left_string);
	g_free (info->header_right_string);
	g_free (info->footer_left_string);
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

        info = g_new0 (EmbedPrintInfo, 1);

	ephy_dialog_get_value (dialog, PRINTON_PROP, &print_to_file);
        info->print_to_file = g_value_get_int (&print_to_file);

	ephy_dialog_get_value (dialog, PRINTER_PROP, &printer);
	info->printer = g_strdup (g_value_get_string (&printer));

	ephy_dialog_get_value (dialog, FILE_PROP, &file);
        info->file = g_strdup (g_value_get_string (&file));

	ephy_dialog_get_value (dialog, BOTTOM_PROP, &bottom_margin);
        info->bottom_margin = g_value_get_float (&bottom_margin);

	ephy_dialog_get_value (dialog, LEFT_PROP, &left_margin);
        info->left_margin = g_value_get_float (&left_margin);

	ephy_dialog_get_value (dialog, TOP_PROP, &top_margin);
        info->top_margin = g_value_get_float (&top_margin);

	ephy_dialog_get_value (dialog, RIGHT_PROP, &right_margin);
        info->right_margin = g_value_get_float (&right_margin);

	ephy_dialog_get_value (dialog, FROM_PROP, &from_page);
        info->from_page = g_value_get_float (&from_page);

	ephy_dialog_get_value (dialog, TO_PROP, &to_page);
        info->to_page = g_value_get_float (&to_page);

	ephy_dialog_get_value (dialog, PAPER_PROP, &paper);
        info->paper = g_value_get_int (&paper);

	ephy_dialog_get_value (dialog, ALL_PAGES_PROP, &pages);
        info->pages = g_value_get_int (&pages);

	ephy_dialog_get_value (dialog, COLOR_PROP, &print_color);
        info->print_color = !g_value_get_int (&print_color);

	ephy_dialog_get_value (dialog, ORIENTATION_PROP, &orientation);
        info->orientation = g_value_get_int (&orientation);

        info->frame_type = 0;

	ephy_dialog_get_value (dialog, PAGE_TITLE_PROP, &page_title);
        info->header_left_string = g_value_get_boolean (&page_title) ?
				   g_strdup ("&T") : g_strdup ("");

	ephy_dialog_get_value (dialog, PAGE_URL_PROP, &page_url);
        info->header_right_string = g_value_get_boolean (&page_url) ?
				    g_strdup ("&U") : g_strdup ("");

	ephy_dialog_get_value (dialog, PAGE_NUMBERS_PROP, &page_numbers);
        info->footer_left_string = g_value_get_boolean (&page_numbers) ?
				   g_strdup ("&PT") : g_strdup ("");

	ephy_dialog_get_value (dialog, DATE_PROP, &date);
        info->footer_right_string = g_value_get_boolean (&date) ?
				    g_strdup ("&D") : g_strdup ("");

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

	if(PRINT_DIALOG(dialog)->only_collect_info && PRINT_DIALOG(dialog)->ret_info)
	{
		*(PRINT_DIALOG(dialog)->ret_info) = info;
	}
	else
	{
		embed = ephy_embed_dialog_get_embed
			(EPHY_EMBED_DIALOG(dialog));

		info->preview = FALSE;
		ephy_embed_print (embed, info);
		print_free_info (info);
	}

	g_object_unref (G_OBJECT(dialog));
}

static void
print_dialog_preview (EphyDialog *dialog)
{
	EmbedPrintInfo *info;
	EphyEmbed *embed;

	info = print_get_info (dialog);

	if(PRINT_DIALOG(dialog)->only_collect_info && PRINT_DIALOG(dialog)->ret_info)
	{
		*(PRINT_DIALOG(dialog)->ret_info) = info;
	}
	else
	{
		embed = ephy_embed_dialog_get_embed
			(EPHY_EMBED_DIALOG(dialog));

		info->preview = TRUE;
		ephy_embed_print (embed, info);
		print_free_info (info);
	}
	g_signal_emit (G_OBJECT (dialog), print_dialog_signals[PREVIEW], 0);

	g_object_unref (G_OBJECT(dialog));
}

void
print_cancel_button_cb (GtkWidget *widget,
			EphyDialog *dialog)
{
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
	//FIXME: Don't show preview button at all.
	if(!(PRINT_DIALOG(dialog)->only_collect_info))
		print_dialog_preview (dialog);
}


