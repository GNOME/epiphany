/* 
 *  Copyright © 2003 Crispin Flowerday <gnome@flowerday.cx>
 *  Copyright © 2005, 2006 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifndef DONT_HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#ifdef ENABLE_KEYRING
#include <gnome-keyring.h>
#endif

#include "ephy-gui.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-state.h"

#include "ephy-password-dialog.h"

enum
{
	CHECK_USER		= 1 << 0,
	CHECK_DOMAIN		= 1 << 1,
	CHECK_PWD		= 1 << 2,
	CHECK_PWD_MATCH		= 1 << 3,
	CHECK_PWD_QUALITY	= 1 << 4,
	CHECK_MASK		= 0x1f
};

enum
{
	USERNAME_ENTRY,
	DOMAIN_ENTRY,
	PASSWORD_ENTRY,
	NEW_PASSWORD_ENTRY,
	CONFIRM_PASSWORD_ENTRY,
	N_ENTRIES
};

#define EPHY_PASSWORD_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_PASSWORD_DIALOG, EphyPasswordDialogPrivate))

struct _EphyPasswordDialogPrivate
{
	GtkWidget *entry[N_ENTRIES];
	GtkWidget *quality_meter;
	GtkWidget *remember_button[3];
	gdouble quality_minimum;
#if 0
	char *realm;
	char *keyring;

	gpointer lookup_op;

	guint keyring_enabled : 1;
#endif
	EphyPasswordDialogFlags flags;
	guint checks : 5;
	guint track_capslock : 1;
};

enum
{
	PROP_0,
	PROP_FLAGS,
	PROP_DOMAIN,
	PROP_KEYRING,
	PROP_KEYRING_ENABLED
};

G_DEFINE_TYPE (EphyPasswordDialog, ephy_password_dialog, GTK_TYPE_MESSAGE_DIALOG)

/**
 *  Calculate the quality of a password. The algorithm used is taken
 *  directly from mozilla:
 *  mozilla/security/manager/pki/resources/content/password.js
 */
static gdouble
password_quality (const char *text)
{
	const char *p;
	gsize length;
	int uppercase = 0, symbols = 0, numbers = 0, strength;
	gunichar uc;

	if (text == NULL) return 0.0;

	/* Get the length */
	length = g_utf8_strlen (text, -1);

	/* Count the number of number, symbols and uppercase chars */
	for (p = text; *p; p = g_utf8_find_next_char (p, NULL))
	{
		uc = g_utf8_get_char (p);

		if (g_unichar_isdigit (uc))
		{
			numbers++;
		}
		else if (g_unichar_isupper (uc))
		{
			uppercase++;
		}
		else if (g_unichar_islower (uc))
		{
			/* Not counted */
		}
		else if (g_unichar_isgraph (uc))
		{
			symbols++;
		}
	}

	if (length    > 5) length = 5;
	if (numbers   > 3) numbers = 3;
	if (symbols   > 3) symbols = 3;
	if (uppercase > 3) uppercase = 3;
	
	strength = (length * 10 - 20) + (numbers * 10) + (symbols * 15) + (uppercase * 10);
	strength = CLAMP (strength, 0, 100);

	return ((gdouble) strength) / 100.0;
}

static void
update_responses (EphyPasswordDialog *dialog)
{
	EphyPasswordDialogPrivate *priv = dialog->priv;

	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
					   GTK_RESPONSE_ACCEPT,
					   priv->checks == 0);
}

static void
entry_changed_cb (GtkWidget *entry,
		  EphyPasswordDialog *dialog)
{
	EphyPasswordDialogPrivate *priv = dialog->priv;
	guint flag = 0;
	const char *text;

	if (entry == priv->entry[USERNAME_ENTRY])
	{
		flag = CHECK_USER;
	}
	else if (entry == priv->entry[DOMAIN_ENTRY])
	{
		flag = CHECK_DOMAIN;
	}
	else if (entry == priv->entry[PASSWORD_ENTRY])
	{
		flag = CHECK_PWD;
	}

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	if (text != NULL && text[0] != '\0')
	{
		priv->checks &= ~flag;
	}
	else
	{
		priv->checks |= flag;
	}

	update_responses (dialog);
}

static void
password_entry_changed_cb (GtkWidget *entry,
			   EphyPasswordDialog *dialog)
{
	EphyPasswordDialogPrivate *priv = dialog->priv;
	const char *text1, *text2;

	text1 = gtk_entry_get_text (GTK_ENTRY (priv->entry[NEW_PASSWORD_ENTRY]));
	text2 = gtk_entry_get_text (GTK_ENTRY (priv->entry[CONFIRM_PASSWORD_ENTRY]));

	if (text1 != NULL && text2 != NULL && strcmp (text1, text2) == 0)
	{
		priv->checks &= ~CHECK_PWD_MATCH;
	}
	else
	{
		priv->checks |= CHECK_PWD_MATCH;
	}

	if ((priv->flags & EPHY_PASSWORD_DIALOG_FLAGS_SHOW_QUALITY_METER) &&
	    (entry == priv->entry[NEW_PASSWORD_ENTRY]))
	{
		gdouble quality;

		quality = password_quality (text1);

		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->quality_meter),
					       quality);

		if (quality >= priv->quality_minimum)
		{
			priv->checks &= ~CHECK_PWD_QUALITY;
		}
		else
		{
			priv->checks |= CHECK_PWD_QUALITY;
		}
	}

	update_responses (dialog);
}

/* Focuses the next entry */
static void
entry_activate_cb (GtkWidget *entry,
		   EphyPasswordDialog *dialog)
{
	EphyPasswordDialogPrivate *priv = dialog->priv;
	int i;

	for (i = 0; i < N_ENTRIES; ++i)
		if (entry == priv->entry[i]) break;
	g_assert (i < N_ENTRIES);

	++i;
	for ( ; i < N_ENTRIES; ++i)
		if (priv->entry[i] != NULL &&
		    GTK_WIDGET_IS_SENSITIVE (priv->entry[i])) break;

	if (i < N_ENTRIES)
		gtk_widget_grab_focus (priv->entry[i]);
	else
		gtk_window_activate_default (GTK_WINDOW (dialog));
}

static void
ephy_password_dialog_init (EphyPasswordDialog *dialog)
{
	EphyPasswordDialogPrivate *priv;

	priv = dialog->priv = EPHY_PASSWORD_DIALOG_GET_PRIVATE (dialog);

	priv->checks = 0;
	priv->quality_minimum = 0.0;
}

static void
add_row (GtkTable *table,
	 int row,
	 const char *text,
	 GtkWidget *widget)
{
	GtkWidget *label;

	label = gtk_label_new_with_mnemonic (text);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

	gtk_table_attach (table, label,
			  0, 1, row, row + 1,
			  GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach (table, widget,
			  1, 2, row, row + 1,
			  GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
}

static GtkWidget *
new_entry (EphyPasswordDialog *dialog,
	   GtkTable *table,
	   int row,
	   const char *text,
	   gboolean editable,
	   gboolean password,
	   GCallback changed_cb)
{
	GtkWidget *entry;

	entry = gtk_entry_new ();
	gtk_entry_set_visibility (GTK_ENTRY (entry), !password);
	gtk_widget_set_sensitive (entry, editable);
	g_signal_connect (entry, "changed", changed_cb, dialog);
	g_signal_connect (entry, "activate",
			  G_CALLBACK (entry_activate_cb), dialog);

	add_row (table, row, text, entry);

	return entry;
}

#if 0
static void
update_capslock_warning (GtkWidget *widget,
			 gboolean show)
{
	//EphyPasswordDialog *dialog = EPHY_PASSWORD_DIALOG (dialog);
	//EphyPasswordDialogPrivate *priv = dialog->priv;

	g_print ("CapsLock is now %s\n", show?"on": "off");
}

static gboolean
ephy_password_dialog_key_press (GtkWidget *widget,
			        GdkEventKey *event)
{
	/* The documentation says that LOCK_MASK may be either the
	 * Shift or the CapsLock key, but I've only ever seen this set
	 * with CapsLock on. So hopefully this code works ok :)
	 */
	/* Pressing CapsLock when it was off:  state-bit 0 keyval GDK_Caps_Lock
	 * Pressing CapsLock when it was on:   state-bit 1 keyval GDK_Caps_Lock
	 * Pressing any key while CapsLock on: state-bit 1
	 */
	if ((event->state & GDK_LOCK_MASK &&
	    event->keyval != GDK_Caps_Lock) ||
	    event->keyval == GDK_Caps_Lock)
	{
		update_capslock_warning (widget, TRUE);
	}
	else if ((event->state & GDK_LOCK_MASK) == 0 ||
		 ((event->state & GDK_LOCK_MASK) &&
		  event->keyval == GDK_Caps_Lock))
	{
		update_capslock_warning (widget, FALSE);
	}

	return GTK_WIDGET_CLASS (ephy_password_dialog_parent_class)->key_press_event (widget, event);
}
#endif

static GObject *
ephy_password_dialog_constructor (GType type,
				 guint n_construct_properties,
				 GObjectConstructParam *construct_params)

{
	GObject *object;
	GtkWindow *window;
	GtkDialog *dialog;
	GtkMessageDialog *message_dialog;
	EphyPasswordDialog *password_dialog;
	EphyPasswordDialogPrivate *priv;
	GtkWidget *vbox;
	GtkTable *table;
	int row = 0;

	object = G_OBJECT_CLASS (ephy_password_dialog_parent_class)->constructor
			(type, n_construct_properties, construct_params);

	priv = EPHY_PASSWORD_DIALOG (object)->priv;
	window = GTK_WINDOW (object);
	dialog = GTK_DIALOG (object);
	message_dialog = GTK_MESSAGE_DIALOG (object);
	password_dialog = EPHY_PASSWORD_DIALOG (object);

	gtk_window_set_resizable (window, FALSE);
	gtk_box_set_spacing (GTK_BOX (dialog->vbox), 2); /* Message has 24, we want 12 = 2 + 2 * 5 */

	//	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_icon_name (window, "web-browser");

	gtk_image_set_from_icon_name (GTK_IMAGE (message_dialog->image),
				      GTK_STOCK_DIALOG_AUTHENTICATION,
				      GTK_ICON_SIZE_DIALOG);

	vbox = message_dialog->label->parent;

	// fixme resize later
	table = GTK_TABLE (gtk_table_new (6, 2, FALSE));
	gtk_table_set_row_spacings (table, 6);
	gtk_table_set_col_spacings (table, 12);
	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (table), FALSE, FALSE, 0);

	if (priv->flags & EPHY_PASSWORD_DIALOG_FLAGS_SHOW_USERNAME)
	{
		priv->entry[USERNAME_ENTRY] =
			new_entry (password_dialog,
				   table,
				   row++,
				   _("_Username:"),
				   priv->flags & EPHY_PASSWORD_DIALOG_FLAGS_EDIT_USERNAME,
				   FALSE,
				   G_CALLBACK (entry_changed_cb));

		priv->checks |= CHECK_USER;
	}

	if (priv->flags & EPHY_PASSWORD_DIALOG_FLAGS_SHOW_DOMAIN)
	{
		priv->entry[DOMAIN_ENTRY] =
			new_entry (password_dialog,
				   table,
				   row++,
				   _("_Domain:"),
				   priv->flags & EPHY_PASSWORD_DIALOG_FLAGS_EDIT_DOMAIN,
				   FALSE,
				   G_CALLBACK (entry_changed_cb));

		priv->checks |= CHECK_DOMAIN;
	}

	if (priv->flags & EPHY_PASSWORD_DIALOG_FLAGS_SHOW_PASSWORD)
	{
		priv->entry[PASSWORD_ENTRY] =
			new_entry (password_dialog,
				   table,
				   row++,
				   _("_Password:"),
				   TRUE,
				   TRUE,
				   G_CALLBACK (entry_changed_cb));

		priv->checks |= CHECK_PWD;
	}

	if (priv->flags & EPHY_PASSWORD_DIALOG_FLAGS_SHOW_NEW_PASSWORD)
	{
		priv->entry[NEW_PASSWORD_ENTRY] =
			new_entry (password_dialog,
				   table,
				   row++,
				   priv->flags & EPHY_PASSWORD_DIALOG_FLAGS_SHOW_PASSWORD ?
					_("_New password:") :
				   	_("_Password:"),
				   TRUE,
				   TRUE,
				   G_CALLBACK (password_entry_changed_cb));

		priv->entry[CONFIRM_PASSWORD_ENTRY] =
			new_entry (password_dialog,
				   table,
				   row++,
				   _("Con_firm password:"),
				   TRUE,
				   TRUE,
				   G_CALLBACK (password_entry_changed_cb));

		priv->checks |= CHECK_PWD_MATCH;
	}

	if (priv->flags & (EPHY_PASSWORD_DIALOG_FLAGS_SHOW_PASSWORD |
			   EPHY_PASSWORD_DIALOG_FLAGS_SHOW_NEW_PASSWORD))
	{
		priv->track_capslock = TRUE;
//		gtk_table_attach (table, widget,
//				  1, 2, row, row + 1,
//				  GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

	}

	/* Password quality meter */
	/* TODO: We need a better password quality meter */
	if (priv->flags & EPHY_PASSWORD_DIALOG_FLAGS_SHOW_NEW_PASSWORD &&
	    priv->flags & EPHY_PASSWORD_DIALOG_FLAGS_SHOW_QUALITY_METER)
	{
		priv->quality_meter = gtk_progress_bar_new ();
		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->quality_meter), 0.0);

		add_row (table, row++, _("Password quality:"), priv->quality_meter);

		priv->checks |= CHECK_PWD_QUALITY;
	}

	/* Removed unused table rows */
	gtk_table_resize (table, row, 2);

	if (priv->flags & EPHY_PASSWORD_DIALOG_FLAGS_SHOW_REMEMBER)
	{
		GSList *group = NULL;
		GtkWidget *rbox;

		rbox = gtk_vbox_new (FALSE, 6);
		gtk_box_pack_start (GTK_BOX (vbox), rbox, FALSE, FALSE, 0);

		priv->remember_button[0] = gtk_radio_button_new_with_mnemonic (group, _("Do not remember this password"));
		gtk_box_pack_start (GTK_BOX (rbox), priv->remember_button[0], FALSE, FALSE, 0);
		group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (priv->remember_button[0]));

		priv->remember_button[1] = gtk_radio_button_new_with_mnemonic (group, _("_Remember password for this session"));
		gtk_box_pack_start (GTK_BOX (rbox), priv->remember_button[1], FALSE, FALSE, 0);
		group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (priv->remember_button[1]));

		priv->remember_button[2] = gtk_radio_button_new_with_mnemonic (group, _("Save password in _keyring"));
		gtk_box_pack_start (GTK_BOX (rbox), priv->remember_button[2], FALSE, FALSE, 0);

		gtk_widget_set_no_show_all (rbox, !gnome_keyring_is_available ());
	}

	gtk_dialog_add_button (dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (dialog, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_ACCEPT);
	update_responses (password_dialog);

	gtk_widget_show_all (vbox);

	return object;
}

static void
ephy_password_dialog_finalize (GObject *object)
{
//	EphyPasswordDialog *dialog = EPHY_PASSWORD_DIALOG (object);
//	EphyPasswordDialogPrivate *priv = dialog->priv;

	G_OBJECT_CLASS (ephy_password_dialog_parent_class)->finalize (object);
}

static void
ephy_password_dialog_get_property (GObject *object,
				  guint prop_id,
				  GValue *value,
				  GParamSpec *pspec)
{
	/* no readable properties */
	g_assert_not_reached ();
}

static void
ephy_password_dialog_set_property (GObject *object,
				  guint prop_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	EphyPasswordDialogPrivate *priv = EPHY_PASSWORD_DIALOG (object)->priv;

	switch (prop_id)
	{
		case PROP_FLAGS:
			priv->flags = g_value_get_flags (value);
			break;
#if 0
		case PROP_DOMAIN:
			priv->realm = g_value_dup_string (value);
			break;
		case PROP_KEYRING:
			priv->keyring = g_value_dup_string (value);
			break;
		case PROP_KEYRING_ENABLED:
			priv->keyring_enabeld = g_value_get_boolean (value) != FALSE;
			break;
#endif
	}
}

static void
ephy_password_dialog_class_init (EphyPasswordDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
//	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
//	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

	object_class->constructor = ephy_password_dialog_constructor;
	object_class->finalize = ephy_password_dialog_finalize;
	object_class->get_property = ephy_password_dialog_get_property;
	object_class->set_property = ephy_password_dialog_set_property;

//	widget_class->key_press_event = ephy_password_dialog_key_press;

//	dialog_class->response = ephy_password_dialog_response;

	g_type_class_add_private (object_class, sizeof (EphyPasswordDialogPrivate));

	g_object_class_install_property
		(object_class,
		 PROP_FLAGS,
		 g_param_spec_flags ("flags",
				     "flags",
				     "flags",
				     EPHY_TYPE_PASSWORD_DIALOG_FLAGS,
				     EPHY_PASSWORD_DIALOG_FLAGS_DEFAULT,
				     G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
				     G_PARAM_CONSTRUCT_ONLY));
#if 0
	g_object_class_install_property (object_class,
					 PROP_DOMAIN,
					 g_param_spec_string ("realm",
							      "realm",
							      "realm",
							      NULL,
							      G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							      G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_KEYRING,
					 g_param_spec_string ("keyring",
							      "keyring",
							      "keyring",
							      NULL,
							      G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							      G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_KEYRING_ENABLED,
					 g_param_spec_boolean ("keyring-enabled",
							       "keyring-enabled",
							       "keyring-enabled",
							       TRUE,
							       G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							       G_PARAM_CONSTRUCT_ONLY));
#endif
}

GtkWidget *
ephy_password_dialog_new (GtkWidget *parent,
			  const char *title,
			  EphyPasswordDialogFlags flags)
{
	return g_object_new (EPHY_TYPE_PASSWORD_DIALOG,
			     "transient-for", parent,
			     "title", title,
			     "message-type", GTK_MESSAGE_OTHER,
			     "flags", flags,
			     (gpointer) NULL);
}

void
ephy_password_dialog_set_remember (EphyPasswordDialog *dialog,
				   GnomePasswordDialogRemember remember)
{
	EphyPasswordDialogPrivate *priv;

	g_return_if_fail (EPHY_IS_PASSWORD_DIALOG (dialog));
	g_return_if_fail (remember < GNOME_PASSWORD_DIALOG_REMEMBER_NOTHING ||
			  remember > GNOME_PASSWORD_DIALOG_REMEMBER_FOREVER);

	priv = dialog->priv;
	g_return_if_fail (priv->flags & EPHY_PASSWORD_DIALOG_FLAGS_SHOW_REMEMBER);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->remember_button[remember]), TRUE);
}

GnomePasswordDialogRemember
ephy_password_dialog_get_remember (EphyPasswordDialog *dialog)
{
	EphyPasswordDialogPrivate *priv;
	GnomePasswordDialogRemember remember = GNOME_PASSWORD_DIALOG_REMEMBER_NOTHING;

	g_return_val_if_fail (EPHY_IS_PASSWORD_DIALOG (dialog), remember);

	priv = dialog->priv;
	g_return_val_if_fail (priv->flags & EPHY_PASSWORD_DIALOG_FLAGS_SHOW_REMEMBER, remember);

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->remember_button[0])))
		remember = GNOME_PASSWORD_DIALOG_REMEMBER_NOTHING;
	else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->remember_button[1])))
		remember = GNOME_PASSWORD_DIALOG_REMEMBER_SESSION;
	else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->remember_button[2])))
		remember = GNOME_PASSWORD_DIALOG_REMEMBER_FOREVER;

	return remember;
}

const char *
ephy_password_dialog_get_username (EphyPasswordDialog *dialog)
{
	EphyPasswordDialogPrivate *priv;

	g_return_val_if_fail (EPHY_IS_PASSWORD_DIALOG (dialog), NULL);

	priv = dialog->priv;
	g_return_val_if_fail (priv->flags & EPHY_PASSWORD_DIALOG_FLAGS_SHOW_USERNAME, NULL);

	return gtk_entry_get_text (GTK_ENTRY (priv->entry[USERNAME_ENTRY]));
}

const char *
ephy_password_dialog_get_domain (EphyPasswordDialog *dialog)
{
	EphyPasswordDialogPrivate *priv;

	g_return_val_if_fail (EPHY_IS_PASSWORD_DIALOG (dialog), NULL);

	priv = dialog->priv;
	g_return_val_if_fail (priv->flags & EPHY_PASSWORD_DIALOG_FLAGS_SHOW_DOMAIN, NULL);

	return gtk_entry_get_text (GTK_ENTRY (priv->entry[DOMAIN_ENTRY]));
}

const char *
ephy_password_dialog_get_password (EphyPasswordDialog *dialog)
{
	EphyPasswordDialogPrivate *priv;

	g_return_val_if_fail (EPHY_IS_PASSWORD_DIALOG (dialog), NULL);

	priv = dialog->priv;
	g_return_val_if_fail (priv->flags & EPHY_PASSWORD_DIALOG_FLAGS_SHOW_PASSWORD, NULL);

	return gtk_entry_get_text (GTK_ENTRY (priv->entry[PASSWORD_ENTRY]));
}

const char *
ephy_password_dialog_get_new_password (EphyPasswordDialog *dialog)
{
	EphyPasswordDialogPrivate *priv;

	g_return_val_if_fail (EPHY_IS_PASSWORD_DIALOG (dialog), NULL);

	priv = dialog->priv;
	g_return_val_if_fail (priv->flags & EPHY_PASSWORD_DIALOG_FLAGS_SHOW_NEW_PASSWORD, NULL);

	return gtk_entry_get_text (GTK_ENTRY (priv->entry[NEW_PASSWORD_ENTRY]));
}

void
ephy_password_dialog_fill (EphyPasswordDialog *dialog,
			   GList *attributes_list)
{
	
}
