/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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
 *  $Id$
 */

#include "config.h"

#include "ephy-dialog.h"
#include "ephy-state.h"
#include "ephy-gui.h"
#include "eel-gconf-extensions.h"
#include "ephy-debug.h"

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

enum
{
	PROP_0,
	PROP_PARENT_WINDOW,
	PROP_MODAL,
	PROP_PERSIST_POSITION,
	PROP_DEFAULT_WIDTH,
	PROP_DEFAULT_HEIGHT
};

typedef enum
{
	PT_TOGGLEBUTTON,
	PT_RADIOBUTTON,
	PT_SPINBUTTON,
	PT_COMBOBOX,
	PT_EDITABLE,
	PT_UNKNOWN
} WidgetType;

typedef struct
{
	const char *id;
	EphyDialog *dialog;
	char *pref;
	EphyDialogApplyType apply_type;
	GtkWidget *widget;
	WidgetType widget_type;
	GType data_type;
	GList *string_enum;
	int data_col;
	gboolean loaded;
	gboolean sane_state;
} PropertyInfo;

#define EPHY_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_DIALOG, EphyDialogPrivate))

struct _EphyDialogPrivate
{
	char *name;

	GHashTable *props;
	GtkWidget *parent;
	GtkWidget *dialog;

	guint modal : 1;
	guint has_default_size : 1;
	guint disposing : 1;
	guint initialized : 1;
	guint persist_position : 1;
	int default_width;
	int default_height;
};

#define SPIN_DELAY 0.20

enum
{
	CHANGED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };

static void ephy_dialog_class_init (EphyDialogClass *klass);
static void ephy_dialog_init	   (EphyDialog *window);

static GObjectClass *parent_class = NULL;

GType
ephy_dialog_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EphyDialogClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ephy_dialog_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (EphyDialog),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_dialog_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EphyDialog",
					       &our_info, 0);
	}

	return type;
}

static PropertyInfo *
lookup_info (EphyDialog *dialog, const char *id)
{
	return g_hash_table_lookup (dialog->priv->props, id);
}

static void
set_sensitivity (PropertyInfo *info, gboolean sensitive)
{
	g_return_if_fail (info->widget != NULL);

	if (info->widget_type == PT_RADIOBUTTON)
	{
		GSList *list, *l;

		list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (info->widget));

		for (l = list; l != NULL; l = l->next)
		{
			gtk_widget_set_sensitive (GTK_WIDGET (l->data), sensitive);
		}
	}
	else if (info->widget_type == PT_EDITABLE)
	{
		gtk_editable_set_editable (GTK_EDITABLE (info->widget), sensitive);
	}
	else
	{
		gtk_widget_set_sensitive (info->widget, sensitive);
	}
}

static void
set_value_from_pref (PropertyInfo *info, GValue *value)
{
	char *text;

	switch (info->data_type)
	{
		case G_TYPE_STRING:
			g_value_init (value, G_TYPE_STRING);
			text = eel_gconf_get_string (info->pref);
			g_value_take_string (value, text);
			break;
		case G_TYPE_INT:
			g_value_init (value, G_TYPE_INT);
			g_value_set_int (value, eel_gconf_get_integer (info->pref));
			break;
		case G_TYPE_FLOAT:
			g_value_init (value, G_TYPE_FLOAT);
			g_value_set_float (value, eel_gconf_get_float (info->pref));
			break;
		case G_TYPE_BOOLEAN:
			g_value_init (value, G_TYPE_BOOLEAN);
			g_value_set_boolean (value, eel_gconf_get_boolean (info->pref));
			break;
		default:
			g_warning ("Unsupported value read from pref %s\n", info->pref);
			break;
	}
}

static void
set_pref_from_value (PropertyInfo *info, GValue *value)
{
	const char *pref = info->pref;

	if (!G_VALUE_HOLDS (value, info->data_type))
	{
		g_warning ("Value type mismatch for id[%s], pref[%s]", info->id, info->pref);
		return;
	}

	switch (info->data_type)
	{
		case G_TYPE_STRING:
		{
			const char *string = g_value_get_string (value);
			if (string != NULL)
			{
				eel_gconf_set_string (pref, string);
			}
			else
			{
				eel_gconf_unset_key (pref);
			}
			break;
		}
		case G_TYPE_INT:
			eel_gconf_set_integer (pref, g_value_get_int (value));
			break;
		case G_TYPE_FLOAT:
			eel_gconf_set_float (pref, g_value_get_float (value));
			break;
		case G_TYPE_BOOLEAN:
			eel_gconf_set_boolean (pref, g_value_get_boolean (value));
			break;
		default:
			break;
	}
}

static gboolean
set_value_from_editable (PropertyInfo *info, GValue *value)
{
	char *text;
	gboolean retval = TRUE;
	gboolean free_text = TRUE;

	g_return_val_if_fail (GTK_IS_EDITABLE (info->widget), FALSE);

	text = gtk_editable_get_chars (GTK_EDITABLE (info->widget), 0, -1);

	g_value_init (value, info->data_type);
	switch (info->data_type)
	{
		case G_TYPE_STRING:
			g_value_take_string (value, text);
			free_text = FALSE;
			break;
		/* FIXME : handle possible errors in the input for int and float */
		case G_TYPE_INT:
			g_value_set_int (value, atoi (text));
			break;
		case G_TYPE_FLOAT:
			g_value_set_float (value, strtod (text, NULL));
			break;
		default:
			retval = FALSE;
			g_value_unset (value);
			g_warning ("Unsupported value type for editable %s", info->id);
			break;
	}

	if (free_text)
	{
		g_free (text);
	}

	return retval;
}

static gboolean
set_value_from_combobox (PropertyInfo *info, GValue *value)
{
	g_return_val_if_fail (GTK_IS_COMBO_BOX (info->widget), FALSE);

	if (info->data_col != -1)
	{
		GtkTreeModel *model;
		GtkTreeIter iter;

		if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (info->widget), &iter))
		{
			model = gtk_combo_box_get_model (GTK_COMBO_BOX (info->widget));
			gtk_tree_model_get_value (model, &iter, info->data_col, value);

			return TRUE;
		}
	}
	else if (info->data_type == G_TYPE_INT)
	{
		int index;

		index = gtk_combo_box_get_active (GTK_COMBO_BOX (info->widget));

		if (index >= 0)
		{
			g_value_init (value, G_TYPE_INT);
			g_value_set_int (value, index);

			return TRUE;
		}
	}
	else
	{
		g_warning ("Unsupported data type for combo %s\n", info->id);
	}

	return FALSE;
}

static int
get_radio_button_active_index (GtkWidget *radiobutton)
{
	GtkToggleButton *toggle_button;
	GSList *list;
	int index, i, length;

	/* get group list */
	list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
	length = g_slist_length (list);

	/* iterate over list to find active button */
	for (i = 0; list != NULL; i++, list = list->next)
	{
		/* get button and text */
		toggle_button = GTK_TOGGLE_BUTTON (list->data);
		if (gtk_toggle_button_get_active (toggle_button))
		{
			break;
		}
	}

	/* check we didn't run off end */
	g_assert (list != NULL);

	/* return index (reverse order!) */
	return index = (length - 1) - i;
}

static gboolean
set_value_from_radiobuttongroup (PropertyInfo *info, GValue *value)
{
	gboolean retval = TRUE;
	int index;

	g_return_val_if_fail (GTK_IS_RADIO_BUTTON (info->widget), FALSE);

	index = get_radio_button_active_index (info->widget);
	g_return_val_if_fail (index >= 0, FALSE);

	if (info->data_type == G_TYPE_STRING)
	{
		g_return_val_if_fail (info->string_enum != NULL, FALSE);
		g_return_val_if_fail (g_list_nth_data (info->string_enum, index) != NULL, FALSE);

		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, (char*) g_list_nth_data (info->string_enum, index));
	}
	else if (info->data_type == G_TYPE_INT)
	{
		g_value_init (value, G_TYPE_INT);
		g_value_set_int (value, index);
	}
	else
	{
		retval = FALSE;
		g_warning ("unsupported data type for radio button %s\n", info->id);
	}

	return retval;
}

static gboolean
set_value_from_spin_button (PropertyInfo *info, GValue *value)
{
	gboolean retval = TRUE;
	gdouble f;
	gboolean is_int;

	g_return_val_if_fail (GTK_IS_SPIN_BUTTON (info->widget), FALSE);

	f = gtk_spin_button_get_value (GTK_SPIN_BUTTON (info->widget));

	is_int = (gtk_spin_button_get_digits (GTK_SPIN_BUTTON(info->widget)) == 0);

	if (info->data_type == G_TYPE_INT && is_int)
	{
		g_value_init (value, G_TYPE_INT);
		g_value_set_int (value, (int) f);
	}
	else if (info->data_type == G_TYPE_FLOAT)
	{
		g_value_init (value, G_TYPE_FLOAT);
		g_value_set_float (value, f);
	}
	else
	{
		retval = FALSE;
		g_warning ("Unsupported data type for spin button %s\n", info->id);
	}

	return retval;
}

static gboolean
set_value_from_togglebutton (PropertyInfo *info, GValue *value)
{
	gboolean retval = TRUE;
	gboolean active;

	g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (info->widget), FALSE);

	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (info->widget));

	if (info->apply_type & PT_INVERTED)
	{
		active = !active;
	}

	if (info->data_type == G_TYPE_BOOLEAN)
	{
		g_value_init (value, info->data_type);
		g_value_set_boolean (value, active);
	}
	else
	{
		retval = FALSE;
		g_warning ("Unsupported data type for toggle button %s\n", info->id);
	}

	return retval;
}

static gboolean
set_value_from_info (PropertyInfo *info, GValue *value)
{
	gboolean retval;

	if (info->sane_state == FALSE)
	{
		return FALSE;
	}

	switch (info->widget_type)
	{
		case PT_SPINBUTTON:
			retval = set_value_from_spin_button (info, value);
			break;
		case PT_RADIOBUTTON:
			retval = set_value_from_radiobuttongroup (info, value);
			break;
		case PT_TOGGLEBUTTON:
			retval = set_value_from_togglebutton (info, value);
			break;
		case PT_EDITABLE:
			retval = set_value_from_editable (info, value);
			break;
		case PT_COMBOBOX:
			retval = set_value_from_combobox (info, value);
			break;
		default:
			retval = FALSE;
			g_warning ("Unsupported widget type\n");
			break;
	}

	return retval;
}

static void
set_editable_from_value (PropertyInfo *info, const GValue *value)
{
	char *text = NULL;
	int pos = 0; /* insertion position */

	g_return_if_fail (GTK_IS_EDITABLE (info->widget));

	switch (info->data_type)
	{
		case G_TYPE_STRING:
			text = g_value_dup_string (value);
			break;
		case G_TYPE_INT:
			text = g_strdup_printf ("%d", g_value_get_int (value));
			break;
		case G_TYPE_FLOAT:
			text = g_strdup_printf ("%.2f", g_value_get_float (value));
			break;
		default:
			break;
	}

	if (text == NULL)
	{
		text = g_strdup ("");
	}

	info->sane_state = TRUE;

	gtk_editable_delete_text (GTK_EDITABLE (info->widget), 0, -1);
	gtk_editable_insert_text (GTK_EDITABLE (info->widget), text, strlen (text), &pos);

	g_free (text);
}

static int
strcmp_with_null (const char *key1,
		  const char *key2)
{
	if (key1 == NULL && key2 == NULL)
	{
		return 0;
	}
	if (key1 == NULL)
	{
		return -1;
	}
	if (key2 == NULL)
	{
		return 1;
	}

	return strcmp (key1, key2);
}

static int
get_index_from_value (const GValue *value, GList *string_enum)
{
	int index = -1;
	const char *val;
	GList *s = NULL;

	if (string_enum)
	{
		val = g_value_get_string (value);

		s = g_list_find_custom (string_enum, val, (GCompareFunc) strcmp_with_null);

		if (s)
		{
			index = g_list_position (string_enum, s);
		}

	}
	else
	{
		index = g_value_get_int (value);
	}

	return index;
}

static gboolean
compare_values (const GValue *a, const GValue *b)
{
	if (G_VALUE_HOLDS (a, G_TYPE_STRING))
	{
		const char *ta, *tb;

		ta = g_value_get_string (a);
		tb = g_value_get_string (b);

		return (strcmp_with_null (ta, tb) == 0);
	}
	else if (G_VALUE_HOLDS (a, G_TYPE_INT))
	{
		return g_value_get_int (a) == g_value_get_int (b);
	}
	else if (G_VALUE_HOLDS (a, G_TYPE_FLOAT))
	{
		return g_value_get_float (a) == g_value_get_float (b);
	}
	else if (G_VALUE_HOLDS (a, G_TYPE_BOOLEAN))
	{
		return g_value_get_boolean (a) == g_value_get_boolean (b);
	}

	return FALSE;
}

static void
set_combo_box_from_value (PropertyInfo *info, const  GValue *value)
{
	g_return_if_fail (GTK_IS_COMBO_BOX (info->widget));

	if (info->data_col != -1)
	{
		GValue data = { 0, };
		GtkTreeModel *model;
		GtkTreeIter iter;
		gboolean valid, found = FALSE;

		model = gtk_combo_box_get_model (GTK_COMBO_BOX (info->widget));

		valid = gtk_tree_model_get_iter_first (model, &iter);
		while (valid)
		{
			gtk_tree_model_get_value (model, &iter, info->data_col, &data);
			found = compare_values (&data, value);
			g_value_unset (&data);

			if (found) break;

			valid = gtk_tree_model_iter_next (model, &iter);
		}

		if (found)
		{
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (info->widget), &iter);
		}
		else
		{
			gtk_combo_box_set_active (GTK_COMBO_BOX (info->widget), 0);
		}

		info->sane_state = found;
	}
	else if (info->data_type == G_TYPE_INT)
	{
		int index;

		index = g_value_get_int (value);

		info->sane_state = index >= 0;

		g_return_if_fail (index >= -1);

		gtk_combo_box_set_active (GTK_COMBO_BOX (info->widget), index);
	}
	else
	{
		g_warning ("Unsupported data type for combo box %s\n", info->id);
	}
}

static void
set_radiobuttongroup_from_value (PropertyInfo *info, const GValue *value)
{
	GtkToggleButton *button;
	GSList *list;
	gint length;
	int index;

	g_return_if_fail (GTK_IS_RADIO_BUTTON (info->widget));

	list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (info->widget));

	length = g_slist_length (list);

	index = get_index_from_value (value, info->string_enum);

	/* new buttons are *prepended* to the list, so button added as first
	 * has last position in the list */
	index = (length - 1) - index;

	if (index < 0 || index >= length)
	{
		info->sane_state = FALSE;
		g_return_if_fail (index >= 0 && index < length);
		return;
	}

	button = GTK_TOGGLE_BUTTON (g_slist_nth_data (list, index));
	g_return_if_fail (button != NULL);

	info->sane_state = TRUE;

	if (gtk_toggle_button_get_active (button) == FALSE)
	{
		gtk_toggle_button_set_active (button, TRUE);
	}
}

static void
set_spin_button_from_value (PropertyInfo *info, const GValue *value)
{
	gdouble f = 0.0;
	gboolean is_int;

	g_return_if_fail (GTK_IS_SPIN_BUTTON (info->widget));

	is_int = (gtk_spin_button_get_digits (GTK_SPIN_BUTTON (info->widget)) == 0);

	if (info->data_type == G_TYPE_INT && is_int)
	{
		f = (float) g_value_get_int (value);
	}
	else if (info->data_type == G_TYPE_FLOAT)
	{
		f = g_value_get_float (value);
	}
	else
	{
		info->sane_state = FALSE;
		g_warning ("Unsupported data type for spin button %s\n", info->id);
		return;
	}

	info->sane_state = TRUE;

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (info->widget), f);
}

static void
set_togglebutton_from_value (PropertyInfo *info, const GValue *value)
{
	gboolean active;

	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (info->widget));
	g_return_if_fail (info->data_type == G_TYPE_BOOLEAN);

	active = g_value_get_boolean (value);

	if (info->apply_type & PT_INVERTED)
	{
		active = !active;
	}

	info->sane_state = TRUE;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (info->widget), active);
}

static void
set_info_from_value (PropertyInfo *info, const GValue *value)
{
	if (!G_VALUE_HOLDS (value, info->data_type))
	{
		g_warning ("Incompatible value types for id %s\n", info->id);
		return;
	}

	switch (info->widget_type)
	{
		case PT_SPINBUTTON:
			set_spin_button_from_value (info, value);
			break;
		case PT_RADIOBUTTON:
			set_radiobuttongroup_from_value (info, value);
			break;
		case PT_TOGGLEBUTTON:
			set_togglebutton_from_value (info, value);
			break;
		case PT_EDITABLE:
			set_editable_from_value (info, value);
			break;
		case PT_COMBOBOX:
			set_combo_box_from_value (info, value);
			break;
		default:
			g_warning ("Unknown widget type\n");
			break;
	}
}

/* widget changed callbacks */

static void
set_pref_from_info_and_emit (PropertyInfo *info)
{
	GValue value = { 0, };

	if (!set_value_from_info (info, &value))
	{
		return;
	}

	g_signal_emit (info->dialog, signals[CHANGED], g_quark_from_string (info->id), &value);

	if ((info->apply_type & PT_AUTOAPPLY) && info->pref != NULL)
	{
		set_pref_from_value (info, &value);
	}

	g_value_unset (&value);
}

static void
togglebutton_clicked_cb (GtkWidget *widget, PropertyInfo *info)
{
	info->sane_state = TRUE;

	set_pref_from_info_and_emit (info);
}

static void
radiobutton_clicked_cb (GtkWidget *widget, PropertyInfo *info)
{
	info->sane_state = TRUE;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget)))
	{
		return;
	}

	set_pref_from_info_and_emit (info);
}

static gboolean
spinbutton_timeout_cb (PropertyInfo *info)
{
	GTimer *spin_timer;

	spin_timer = (GTimer *) g_object_get_data (G_OBJECT (info->widget), "timer");

	/* timer still valid? */
	if (spin_timer == NULL)
	{
		/* don't call me again */
		return FALSE;
	}

	/* okay, we're ready to set */
	if (g_timer_elapsed (spin_timer, NULL) >= SPIN_DELAY)
	{
		/* kill off the timer */
		g_timer_destroy (spin_timer);
		g_object_set_data (G_OBJECT (info->widget), "timer", NULL);

		/* HACK update the spinbutton here so that the
		 * changes made directly in the entry are accepted
		 * and set in the pref. Otherwise the old value is used */
		gtk_spin_button_update (GTK_SPIN_BUTTON (info->widget));

		info->sane_state = TRUE;

		set_pref_from_info_and_emit (info);

		/* done, don't run again */
		return FALSE;
	}

	/* not elapsed yet, call me again */
	return TRUE;
}

static void
spinbutton_changed_cb (GtkWidget *widget, PropertyInfo *info)
{
	GTimer *spin_timer;

	if ((info->apply_type & PT_AUTOAPPLY) == 0) return;

	spin_timer = g_object_get_data (G_OBJECT (info->widget), "timer");

	/* destroy an existing timer */
	if (spin_timer != NULL)
	{
		g_timer_destroy (spin_timer);
	}

	/* start tnew timer */
	spin_timer = g_timer_new();
	g_timer_start (spin_timer);
	g_object_set_data (G_OBJECT (info->widget), "timer", spin_timer);

	g_timeout_add (50, (GSourceFunc) spinbutton_timeout_cb, info);
}

static void
changed_cb (GtkWidget *widget, PropertyInfo *info)
{
	info->sane_state = TRUE;

	set_pref_from_info_and_emit (info);
}

static void
connect_signals (gpointer key, PropertyInfo *info, EphyDialog *dialog)
{
	GSList *list;

	g_return_if_fail (info->widget != NULL);

	switch (info->widget_type)
	{
		case PT_TOGGLEBUTTON:
			g_signal_connect (G_OBJECT (info->widget), "clicked",
					  G_CALLBACK (togglebutton_clicked_cb),
					  (gpointer)info);
			break;
		case PT_RADIOBUTTON:
			list = gtk_radio_button_get_group
				(GTK_RADIO_BUTTON (info->widget));
			for (; list != NULL; list = list->next)
			{
				g_signal_connect
					(G_OBJECT (list->data), "clicked",
					 G_CALLBACK (radiobutton_clicked_cb),
					 info);
			}
			break;
		case PT_SPINBUTTON:
			g_signal_connect (G_OBJECT (info->widget), "changed",
					  G_CALLBACK (spinbutton_changed_cb),
					  info);
			break;
		case PT_COMBOBOX:
			g_signal_connect (G_OBJECT (info->widget), "changed",
					  G_CALLBACK (changed_cb), info);
			break;
		case PT_EDITABLE:
			g_signal_connect (G_OBJECT (info->widget), "changed",
					  G_CALLBACK (changed_cb), info);
			break;
		case PT_UNKNOWN:
			break;
	}
}

static void
disconnect_signals (gpointer key, PropertyInfo *info, EphyDialog *dialog)
{
	g_return_if_fail (info->widget != NULL);

	g_signal_handlers_disconnect_matched (info->widget, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, info);
}

static void
init_props (EphyDialog *dialog, const EphyDialogProperty *properties, GtkBuilder *builder)
{
	int i;

	for (i = 0 ; properties[i].id != NULL; i++)
	{
		PropertyInfo *info = g_new0 (PropertyInfo, 1);

		info->id = properties[i].id;
		info->dialog = dialog;
		info->pref = g_strdup (properties[i].pref);
		info->apply_type = properties[i].apply_type;
		info->string_enum = NULL;
		info->data_col = -1;

		info->widget = (GtkWidget*)gtk_builder_get_object (builder, info->id);
		
		if (GTK_IS_COMBO_BOX (info->widget))
		{
			info->widget_type = PT_COMBOBOX;
			info->data_type = G_TYPE_INT;
		}
		else if (GTK_IS_SPIN_BUTTON (info->widget))
		{
			info->widget_type = PT_SPINBUTTON;
			info->data_type = G_TYPE_INT;
		}
		else if (GTK_IS_RADIO_BUTTON (info->widget))
		{
			info->widget_type = PT_RADIOBUTTON;
			info->data_type = G_TYPE_INT;
		}
		else if (GTK_IS_TOGGLE_BUTTON (info->widget))
		{
			info->widget_type = PT_TOGGLEBUTTON;
			info->data_type = G_TYPE_BOOLEAN;
		}
		else if (GTK_IS_EDITABLE (info->widget))
		{
			info->widget_type = PT_EDITABLE;
			info->data_type = G_TYPE_STRING;
		}
		else
		{
			info->widget_type = PT_UNKNOWN;
			info->data_type = G_TYPE_INVALID;
		}

		if (properties[i].data_type != 0)
		{
			info->data_type = properties[i].data_type;
		}

		info->loaded = FALSE;
		info->sane_state = FALSE;

		g_hash_table_insert (dialog->priv->props, (char *) info->id, info);		
	}
}

static void
load_info (gpointer key, PropertyInfo *info, EphyDialog *dialog)
{
	GValue value = { 0, };

	g_return_if_fail (info->widget != NULL);

	if (info->pref != NULL)
	{
		set_value_from_pref (info, &value);
		set_info_from_value (info, &value);

		g_signal_emit (info->dialog, signals[CHANGED], g_quark_from_string (info->id), &value);

		g_value_unset (&value);
	
		set_sensitivity (info, eel_gconf_key_is_writable (info->pref));
	}

	info->loaded = TRUE;
}

static void
save_info (gpointer key, PropertyInfo *info, EphyDialog *dialog)
{
	GValue value = { 0, };

	if (info->pref == NULL || (info->apply_type & PT_NORMAL) == 0)
	{
		return;
	}

	if (!info->sane_state)
	{
		g_warning ("Not persisting insane state of id[%s]", info->id);
		return;
	}

	if (set_value_from_info (info, &value))
	{
		set_pref_from_value (info, &value);
		g_value_unset (&value);
	}
}

static void
setup_default_size (EphyDialog *dialog)
{
	if (dialog->priv->has_default_size == FALSE)
	{
		EphyStateWindowFlags flags;

		flags = EPHY_STATE_WINDOW_SAVE_SIZE;

		if (dialog->priv->persist_position)
		{
			flags |= EPHY_STATE_WINDOW_SAVE_POSITION;
		}

		ephy_state_add_window (dialog->priv->dialog,
				       dialog->priv->name,
				       dialog->priv->default_width,
				       dialog->priv->default_height,
				       FALSE, flags);

		dialog->priv->has_default_size = TRUE;
	}
}

static void
dialog_destroy_cb (GtkWidget *widget, EphyDialog *dialog)
{
	g_hash_table_foreach (dialog->priv->props, (GHFunc) save_info, dialog);

	if (dialog->priv->disposing == FALSE)
	{
		g_object_unref (dialog);
	}
}

static void
impl_construct (EphyDialog *dialog,
		const EphyDialogProperty *properties,
		const char *file,
		const char *name,
		const char *domain)
{
	EphyDialogPrivate *priv = dialog->priv;
	GtkBuilder *builder;
	GError *error = NULL;

	builder = gtk_builder_new ();
	gtk_builder_set_translation_domain (builder, domain);
	gtk_builder_add_from_file (builder, file, &error);
	if (error)
	{
		g_warning ("Unable to load UI file %s: %s", file, error->message);
		g_error_free (error);
		return;
	}

	priv->dialog = (GtkWidget*)gtk_builder_get_object (builder, name);
	g_return_if_fail (priv->dialog != NULL);

	if (priv->name == NULL)
	{
		priv->name = g_strdup (name);
	}

	if (properties)
	{
		init_props (dialog, properties, builder);
	}

	g_signal_connect_object (dialog->priv->dialog, "destroy",
				 G_CALLBACK(dialog_destroy_cb), dialog, 0);

	g_object_unref (builder);
}

static void
impl_show (EphyDialog *dialog)
{
	if (dialog->priv->initialized == FALSE)
	{
		dialog->priv->initialized = TRUE;

		g_hash_table_foreach (dialog->priv->props, (GHFunc) load_info, dialog);
		g_hash_table_foreach (dialog->priv->props, (GHFunc) connect_signals, dialog);
	}

	setup_default_size (dialog);

	if (dialog->priv->parent != NULL)
	{
		/* make the dialog transient again, because it seems to get
		 * forgotten after gtk_widget_hide
		 */
		gtk_window_set_transient_for (GTK_WINDOW (dialog->priv->dialog),
					      GTK_WINDOW (dialog->priv->parent));
	}

	gtk_window_present (GTK_WINDOW (dialog->priv->dialog));
}

void
ephy_dialog_set_modal (EphyDialog *dialog,
		       gboolean is_modal)
{
	dialog->priv->modal = is_modal != FALSE;

	gtk_window_set_modal (GTK_WINDOW(dialog->priv->dialog), is_modal);
}

void
ephy_dialog_add_enum (EphyDialog *dialog,
		      const char *id,
		      guint n_items,
		      const char *const *items)
{
	PropertyInfo *info;
	int i = 0;
	GList *l = NULL;

	info = lookup_info (dialog, id);
	g_return_if_fail (info != NULL);

	for (i = 0; i < n_items; i++)
	{
		l = g_list_prepend (l, g_strdup (items[i]));
	}

	info->string_enum = g_list_reverse (l);
}

void
ephy_dialog_set_data_column (EphyDialog *dialog,
			     const char *id,
			     int column)
{
	PropertyInfo *info;

	info = lookup_info (dialog, id);
	g_return_if_fail (info != NULL);

	info->data_col = column;
}

void
ephy_dialog_set_pref (EphyDialog *dialog,
		      const char *property_id,
		      const char *pref)
{
	PropertyInfo *info;

	info = lookup_info (dialog, property_id);
	g_return_if_fail (info != NULL);

	disconnect_signals (NULL, info, dialog);

	info->loaded = FALSE;
	info->sane_state = FALSE;
	g_free (info->pref);
	info->pref = g_strdup (pref);

	if (dialog->priv->initialized)
	{
		/* dialog is already initialised, so initialise this here */
		load_info (NULL, info, dialog);
		connect_signals (NULL, info, dialog);
	}
}

void
ephy_dialog_set_size_group (EphyDialog *dialog,
			    const char *first_id,
			    ...)
{
	GtkSizeGroup *size_group;
	va_list vl;

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	va_start (vl, first_id);

	while (first_id != NULL)
	{
		PropertyInfo *info;

		info = lookup_info (dialog, first_id);
		g_return_if_fail (info != NULL);

		g_return_if_fail (info->widget != NULL);

		gtk_size_group_add_widget (size_group, info->widget);

		first_id = va_arg (vl, const char*);
	}

	va_end (vl);

	g_object_unref (size_group);
}

void
ephy_dialog_construct (EphyDialog *dialog,
		       const EphyDialogProperty *properties,
		       const char *file,
		       const char *name,
		       const char *domain)
{
	EphyDialogClass *klass = EPHY_DIALOG_GET_CLASS (dialog);
	klass->construct (dialog, properties, file, name, domain);
}

void
ephy_dialog_show (EphyDialog *dialog)
{
	EphyDialogClass *klass = EPHY_DIALOG_GET_CLASS (dialog);
	klass->show (dialog);
}

void
ephy_dialog_hide (EphyDialog *dialog)
{
	g_return_if_fail (EPHY_IS_DIALOG (dialog));
	g_return_if_fail (dialog->priv->dialog != NULL);

	gtk_widget_hide (dialog->priv->dialog);
}

int
ephy_dialog_run (EphyDialog *dialog)
{
	ephy_dialog_show (dialog);

	gtk_window_group_add_window (ephy_gui_ensure_window_group (GTK_WINDOW (dialog->priv->parent)),
				     GTK_WINDOW (dialog->priv->dialog));

	return gtk_dialog_run (GTK_DIALOG (dialog->priv->dialog));
}

GtkWidget *
ephy_dialog_get_control (EphyDialog *dialog,
			 const char *property_id)
{
	PropertyInfo *info;

	info = lookup_info (dialog, property_id);
	g_return_val_if_fail (info != NULL, NULL);

	return info->widget;
}

void
ephy_dialog_get_controls (EphyDialog *dialog,
			  const char *property_id,
			  ...)
{
	PropertyInfo *info;
	GtkWidget **wptr;
	va_list varargs;

	va_start (varargs, property_id);

	while (property_id != NULL)
	{
		info = lookup_info (dialog, property_id);
		g_return_if_fail (info != NULL);

		wptr = va_arg (varargs, GtkWidget **);
		*wptr = info->widget;

		property_id = va_arg (varargs, const char *);
	}

	va_end (varargs);
}

gboolean
ephy_dialog_get_value (EphyDialog *dialog,
		       const char *property_id,
		       GValue *value)
{
	PropertyInfo *info;

	info = lookup_info (dialog, property_id);
	g_return_val_if_fail (info != NULL, FALSE);

	return set_value_from_info (info, value);
}

void
ephy_dialog_set_value (EphyDialog *dialog,
		       const char *property_id,
		       const GValue *value)
{
	PropertyInfo *info;

	info = lookup_info (dialog, property_id);
	g_return_if_fail (info != NULL);

	set_info_from_value (info, value);
}

static void
free_prop_info (PropertyInfo *info)
{
	if (info->string_enum)
	{
		g_list_foreach (info->string_enum, (GFunc)g_free, NULL);
		g_list_free (info->string_enum);
	}

	g_free (info->pref);

	g_free (info);
}

static void
ephy_dialog_init (EphyDialog *dialog)
{
	dialog->priv = EPHY_DIALOG_GET_PRIVATE (dialog);

	dialog->priv->default_width = -1;
	dialog->priv->default_height = -1;

	dialog->priv->props = g_hash_table_new_full 
		(g_str_hash, g_str_equal, NULL, (GDestroyNotify) free_prop_info);
}

static void
ephy_dialog_dispose (GObject *object)
{
	EphyDialog *dialog = EPHY_DIALOG (object);

	if (dialog->priv->dialog)
	{
		dialog->priv->disposing = TRUE;
		gtk_widget_destroy (dialog->priv->dialog);
		dialog->priv->dialog = NULL;
	}

	parent_class->dispose (object);
}

static void
ephy_dialog_finalize (GObject *object)
{
	EphyDialog *dialog = EPHY_DIALOG (object);

	g_hash_table_destroy (dialog->priv->props);

	g_free (dialog->priv->name);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
ephy_dialog_get_parent (EphyDialog *dialog)
{
	g_return_val_if_fail (EPHY_IS_DIALOG (dialog), NULL);

	return dialog->priv->parent;
}

void
ephy_dialog_set_parent (EphyDialog *dialog,
			GtkWidget *parent)
{
	dialog->priv->parent = parent;

	g_object_notify (G_OBJECT (dialog), "parent-window");
}

static void
ephy_dialog_set_property (GObject *object,
			  guint prop_id,
			  const GValue *value,
			  GParamSpec *pspec)
{
	EphyDialog *dialog = EPHY_DIALOG (object);

	switch (prop_id)
	{
		case PROP_PARENT_WINDOW:
			ephy_dialog_set_parent (dialog, g_value_get_object (value));
			break;
		case PROP_MODAL:
			ephy_dialog_set_modal (dialog, g_value_get_boolean (value));
			break;
		case PROP_PERSIST_POSITION:
			dialog->priv->persist_position = g_value_get_boolean (value);
			break;
		case PROP_DEFAULT_WIDTH:
			dialog->priv->default_width = g_value_get_int (value);
			break;
		case PROP_DEFAULT_HEIGHT:
			dialog->priv->default_height = g_value_get_int (value);
			break;
	}
}

static void
ephy_dialog_get_property (GObject *object,
			  guint prop_id,
			  GValue *value,
			  GParamSpec *pspec)
{
	EphyDialog *dialog = EPHY_DIALOG (object);

	switch (prop_id)
	{
		case PROP_PARENT_WINDOW:
			g_value_set_object (value, dialog->priv->parent);
			break;
		case PROP_MODAL:
			g_value_set_boolean (value, dialog->priv->modal);
			break;
		case PROP_PERSIST_POSITION:
			g_value_set_boolean (value, dialog->priv->persist_position);
			break;
		case PROP_DEFAULT_WIDTH:
			g_value_set_int (value, dialog->priv->default_width);
			break;
		case PROP_DEFAULT_HEIGHT:
			g_value_set_int (value, dialog->priv->default_height);
			break;
	}
}

static void
ephy_dialog_class_init (EphyDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_dialog_finalize;
	object_class->dispose = ephy_dialog_dispose;
	object_class->set_property = ephy_dialog_set_property;
	object_class->get_property = ephy_dialog_get_property;

	klass->construct = impl_construct;
	klass->show = impl_show;

	signals[CHANGED] =
		g_signal_new ("changed",
			      EPHY_TYPE_DIALOG,
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
			      G_STRUCT_OFFSET (EphyDialogClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);

	g_object_class_install_property (object_class,
					 PROP_PARENT_WINDOW,
					 g_param_spec_object ("parent-window",
							      "Parent window",
							      "Parent window",
							      GTK_TYPE_WINDOW,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_MODAL,
					 g_param_spec_boolean ("Modal",
							       "Modal",
							       "Modal dialog",
							       FALSE,
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_PERSIST_POSITION,
					 g_param_spec_boolean ("persist-position",
							       "Persist position",
							       "Persist dialog position",
							       FALSE,
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							       G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_DEFAULT_WIDTH,
					 g_param_spec_int ("default-width",
							   "Default width",
							   "Default dialog width",
							   -1,
							   G_MAXINT,
							   -1,
							   G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							   G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_DEFAULT_HEIGHT,
					 g_param_spec_int ("default-height",
							   "Default height",
							   "Default dialog height",
							   -1,
							   G_MAXINT,
							   -1,
							   G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							   G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (EphyDialogPrivate));
}

EphyDialog *
ephy_dialog_new (void)
{
	return EPHY_DIALOG (g_object_new (EPHY_TYPE_DIALOG, NULL));
}

EphyDialog *
ephy_dialog_new_with_parent (GtkWidget *parent_window)
{
	return EPHY_DIALOG (g_object_new (EPHY_TYPE_DIALOG,
					  "parent-window", parent_window,
					  NULL));
}
