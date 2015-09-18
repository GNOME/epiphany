/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2013 Red Hat, Inc.
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#define SECRET_API_SUBJECT_TO_CHANGE
#include <libsecret/secret.h>
#include <webkit2/webkit2.h>

#include "ephy-form-auth-data.h"
#include "ephy-history-service.h"
#include "ephy-embed-shell.h"

#include "clear-data-dialog.h"

struct ClearDataDialogPrivate
{
	GtkWidget *cache_checkbutton;
	GtkWidget *history_checkbutton;
	GtkWidget *passwords_checkbutton;
	GtkWidget *cookies_checkbutton;
	GtkWidget *clear_button;

	ClearDataDialogFlags flags;
	guint num_checked;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClearDataDialog, clear_data_dialog, GTK_TYPE_DIALOG)

static void
clear_data_dialog_class_init (ClearDataDialogClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/epiphany/clear-data-dialog.ui");

	gtk_widget_class_bind_template_child_private (widget_class, ClearDataDialog, cookies_checkbutton);
	gtk_widget_class_bind_template_child_private (widget_class, ClearDataDialog, cache_checkbutton);
	gtk_widget_class_bind_template_child_private (widget_class, ClearDataDialog, passwords_checkbutton);
	gtk_widget_class_bind_template_child_private (widget_class, ClearDataDialog, history_checkbutton);
	gtk_widget_class_bind_template_child_private (widget_class, ClearDataDialog, clear_button);
}

static WebKitCookieManager *
get_cookie_manager (void)
{
	WebKitWebContext *web_context;
	EphyEmbedShell *shell = ephy_embed_shell_get_default ();

	web_context = ephy_embed_shell_get_web_context (shell);;
	return webkit_web_context_get_cookie_manager (web_context);
}

static void
delete_all_passwords (ClearDataDialog *dialog)
{
	GHashTable *attributes;

	attributes = secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA, NULL);
	secret_service_clear (NULL, EPHY_FORM_PASSWORD_SCHEMA,
			      attributes, NULL,
			      (GAsyncReadyCallback)secret_service_clear_finish,
			      NULL);
	g_hash_table_unref (attributes);
}

static void
clear_data_dialog_response_cb (GtkDialog *widget,
			       int response,
			       ClearDataDialog *dialog)
{
	ClearDataDialogPrivate *priv = dialog->priv;

	if (response == GTK_RESPONSE_OK)
	{
		if (gtk_toggle_button_get_active
			(GTK_TOGGLE_BUTTON (priv->history_checkbutton)))
		{
			EphyEmbedShell *shell;
			EphyHistoryService *history;

			shell = ephy_embed_shell_get_default ();
			history = EPHY_HISTORY_SERVICE (ephy_embed_shell_get_global_history_service (shell));
			ephy_history_service_clear (history, NULL, NULL, NULL);
		}
		if (gtk_toggle_button_get_active
			(GTK_TOGGLE_BUTTON (priv->cookies_checkbutton)))
		{
			WebKitCookieManager *cookie_manager;

			cookie_manager = get_cookie_manager ();
			webkit_cookie_manager_delete_all_cookies (cookie_manager);
		}
		if (gtk_toggle_button_get_active
			(GTK_TOGGLE_BUTTON (priv->passwords_checkbutton)))
		{
			delete_all_passwords (dialog);
		}
		if (gtk_toggle_button_get_active
			(GTK_TOGGLE_BUTTON (priv->cache_checkbutton)))
		{
			EphyEmbedShell *shell;
			WebKitFaviconDatabase *database;

			shell = ephy_embed_shell_get_default ();

			ephy_embed_shell_clear_cache (shell);

			database = webkit_web_context_get_favicon_database (ephy_embed_shell_get_web_context (shell));
			webkit_favicon_database_clear (database);
		}
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
checkbutton_toggled_cb (GtkToggleButton *toggle,
			ClearDataDialog *dialog)
{
	if (gtk_toggle_button_get_active (toggle) == TRUE)
	{
		dialog->priv->num_checked++;
	}
	else
	{
		dialog->priv->num_checked--;
	}

	gtk_widget_set_sensitive (dialog->priv->clear_button,
				  dialog->priv->num_checked != 0);
}

static void
update_flags (ClearDataDialog *dialog)
{
	ClearDataDialogPrivate *priv = dialog->priv;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->cookies_checkbutton),
				      (priv->flags & CLEAR_DATA_COOKIES));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->passwords_checkbutton),
				      (priv->flags & CLEAR_DATA_PASSWORDS));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->history_checkbutton),
				      (priv->flags & CLEAR_DATA_HISTORY));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->cache_checkbutton),
				      (priv->flags & CLEAR_DATA_CACHE));
}

static void
setup_page (ClearDataDialog *dialog)
{
	ClearDataDialogPrivate *priv = dialog->priv;

	g_signal_connect (priv->cookies_checkbutton,
			  "toggled",
			  G_CALLBACK (checkbutton_toggled_cb),
			  dialog);
	g_signal_connect (priv->history_checkbutton,
			  "toggled",
			  G_CALLBACK (checkbutton_toggled_cb),
			  dialog);
	g_signal_connect (priv->cache_checkbutton,
			  "toggled",
			  G_CALLBACK (checkbutton_toggled_cb),
			  dialog);
	g_signal_connect (priv->passwords_checkbutton,
			  "toggled",
			  G_CALLBACK (checkbutton_toggled_cb),
			  dialog);

	update_flags (dialog);
}

static void
clear_data_dialog_init (ClearDataDialog *dialog)
{
	dialog->priv = clear_data_dialog_get_instance_private (dialog);
	gtk_widget_init_template (GTK_WIDGET (dialog));

	setup_page (dialog);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (clear_data_dialog_response_cb), dialog);
}

void
clear_data_dialog_set_flags (ClearDataDialog     *dialog,
			     ClearDataDialogFlags flags)
{
	if (dialog->priv->flags != flags)
	{
		dialog->priv->flags = flags;
		update_flags (dialog);
	}
}
