/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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

#include "ephy-dialog.h"
#include "ephy-glade.h"
#include "ephy-state.h"
#include "ephy-gui.h"
#include "eel-gconf-extensions.h"

#include <string.h>
#include <gtk/gtktogglebutton.h>

static void
ephy_dialog_class_init (EphyDialogClass *klass);
static void
ephy_dialog_init (EphyDialog *window);
static void
ephy_dialog_finalize (GObject *object);
static void
ephy_dialog_get_property (GObject *object,
			  guint prop_id,
                          GValue *value,
                          GParamSpec *pspec);
static void
ephy_dialog_set_property (GObject *object,
			  guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec);

static void
ephy_dialog_set_parent (EphyDialog *dialog,
			GtkWidget *parent);

static void
impl_construct (EphyDialog *dialog,
		const EphyDialogProperty *properties,
		const char *file,
		const char *name);
static GtkWidget *
impl_get_control (EphyDialog *dialog,
                  int property_id);
static void
impl_get_value (EphyDialog *dialog,
                int property_id,
                GValue *value);
static gint
impl_run (EphyDialog *dialog);
static void
impl_show (EphyDialog *dialog);
void
ephy_dialog_destroy_cb (GtkWidget *widget,
			EphyDialog *dialog);

enum
{
	PROP_0,
	PROP_PARENT_WINDOW,
	PROP_MODAL
};

typedef enum
{
        PT_TOGGLEBUTTON,
        PT_RADIOBUTTON,
        PT_SPINBUTTON,
        PT_COLOR,
        PT_OPTIONMENU,
        PT_ENTRY,
	PT_UNKNOWN
} PrefType;

typedef struct
{
	int id;
	GtkWidget *widget;
	const char *pref;
	int *sg;
	PropertyType type;
	GList *string_enum;
} PropertyInfo;

struct EphyDialogPrivate
{
	GtkWidget *parent;
	GtkWidget *dialog;
	GtkWidget *container;
	PropertyInfo *props;
	gboolean modal;
	gboolean has_default_size;
	gboolean disposing;
	char *name;

	int spin_item_id;
	GTimer *spin_timer;
	gboolean initialized;
};

#define SPIN_DELAY 0.20

static GObjectClass *parent_class = NULL;

GType
ephy_dialog_get_type (void)
{
        static GType ephy_dialog_type = 0;

        if (ephy_dialog_type == 0)
        {
                static const GTypeInfo our_info =
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

                ephy_dialog_type = g_type_register_static (G_TYPE_OBJECT,
                                                           "EphyDialog",
                                                           &our_info, 0);
        }

        return ephy_dialog_type;
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
	klass->get_control = impl_get_control;
	klass->get_value = impl_get_value;
	klass->run = impl_run;
	klass->show = impl_show;

	g_object_class_install_property (object_class,
                                         PROP_PARENT_WINDOW,
                                         g_param_spec_object ("ParentWindow",
                                                              "ParentWindow",
                                                              "Parent window",
                                                              GTK_TYPE_WIDGET,
                                                              G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
                                         PROP_MODAL,
                                         g_param_spec_boolean ("Modal",
                                                               "Modal",
                                                               "Modal dialog",
                                                               FALSE,
                                                               G_PARAM_READWRITE));
}

static void
set_config_from_editable (GtkWidget *editable, const char *config_name)
{
	GConfValue *gcvalue = eel_gconf_get_value (config_name);
	GConfValueType value_type;
	char *value;
	gint ivalue;
	gfloat fvalue;

	if (gcvalue == NULL) {
		/* ugly hack around what appears to be a gconf bug
		 * it returns a NULL GConfValue for a valid string pref
		 * which is "" by default */
		value_type = GCONF_VALUE_STRING;
	} else {
		value_type = gcvalue->type;
		gconf_value_free (gcvalue);
	}

	/* get all the text into a new string */
	value = gtk_editable_get_chars (GTK_EDITABLE(editable), 0, -1);

	switch (value_type) {
	case GCONF_VALUE_STRING:
		eel_gconf_set_string (config_name,
				      value);
		break;
	/* FIXME : handle possible errors in the input for int and float */
	case GCONF_VALUE_INT:
		ivalue = atoi (value);
		eel_gconf_set_integer (config_name, ivalue);
		break;
	case GCONF_VALUE_FLOAT:
		fvalue = strtod (value, (char**)NULL);
		eel_gconf_set_float (config_name, fvalue);
		break;
	default:
		break;
	}

	/* free the allocated strings */
	g_free (value);
}

static void
set_config_from_optionmenu (GtkWidget *optionmenu, const char *config_name, GList *senum)
{
	int index = gtk_option_menu_get_history (GTK_OPTION_MENU (optionmenu));

	if (senum)
	{
		eel_gconf_set_string (config_name, g_list_nth_data (senum, index));
	}
	else
	{
		eel_gconf_set_integer (config_name, index);
	}
}

static int
get_radio_button_active_index (GtkWidget *radiobutton)
{
	gint index;
	GtkToggleButton *toggle_button;
	gint i, length;
        GSList *list;

	/* get group list */
        list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
        length = g_slist_length (list);

	/* iterate over list to find active button */
	for (i = 0; list != NULL; i++, list = g_slist_next (list))
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

static void
set_config_from_radiobuttongroup (GtkWidget *radiobutton, const char *config_name, GList *senum)
{
	int index;

	index = get_radio_button_active_index (radiobutton);

	if (senum)
	{
		eel_gconf_set_string (config_name, g_list_nth_data (senum, index));
	}
	else
	{
		eel_gconf_set_integer (config_name, index);
	}
}

static void
set_config_from_spin_button (GtkWidget *spinbutton, const char *config_name)
{
	gdouble value;
	gboolean use_int;

	/* read the value as an integer */
	value = gtk_spin_button_get_value (GTK_SPIN_BUTTON(spinbutton));

	use_int = (gtk_spin_button_get_digits (GTK_SPIN_BUTTON(spinbutton)) == 0);

	if (use_int)
	{
		eel_gconf_set_integer (config_name, value);
	}
	else
	{
		eel_gconf_set_float (config_name, value);
	}
}

static void
set_config_from_togglebutton (GtkWidget *togglebutton, const char *config_name)
{
	gboolean value;

	/* read the value */
	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(togglebutton));

	eel_gconf_set_boolean (config_name, value);
}

static void
set_config_from_color (GtkWidget *colorpicker, const char *config_name)
{
	guint8 r, g, b, a;
	gchar color_string[9];

	/* get color values from color picker */
	gnome_color_picker_get_i8 (GNOME_COLOR_PICKER (colorpicker),
				   &r, &g, &b, &a);

	/* write into string (bounded size) */
	g_snprintf (color_string, 9, "#%02X%02X%02X", r, g, b);

	/* set the configuration value */
	eel_gconf_set_string (config_name, color_string);
}

static void
set_editable_from_config (GtkWidget *editable, const char *config_name)
{
	GConfValue *gcvalue = eel_gconf_get_value (config_name);
	GConfValueType value_type;
	gchar *value;

	if (gcvalue == NULL)
	{
		/* ugly hack around what appears to be a gconf bug
		 * it returns a NULL GConfValue for a valid string pref
		 * which is "" by default */
		value_type = GCONF_VALUE_STRING;
	}
	else
	{
		value_type = gcvalue->type;
		gconf_value_free (gcvalue);
	}

	switch (value_type)
	{
	case GCONF_VALUE_STRING:
		value = eel_gconf_get_string (config_name);
		break;
	case GCONF_VALUE_INT:
		value = g_strdup_printf ("%d",eel_gconf_get_integer (config_name));
		break;
	case GCONF_VALUE_FLOAT:
		value = g_strdup_printf ("%.2f",eel_gconf_get_float (config_name));
		break;
	default:
		value = NULL;
	}

	/* set this string value in the widget */
	if (value)
	{
		gtk_entry_set_text(GTK_ENTRY(editable), value);
	}


	/* free the allocated string */
	g_free (value);
}

static int
get_index (const char *config_name, GList *senum)
{
	int index = 0;
	char *val;
	GList *s = NULL;

	if (senum)
	{
		val = eel_gconf_get_string (config_name);

		if (val)
		{
			s = g_list_find_custom (senum, val, (GCompareFunc)strcmp);
			g_free (val);
		}

		if (s)
		{
			index = g_list_position (senum, s);
		}

	}
	else
	{
		index = eel_gconf_get_integer (config_name);
	}

	return index;
}


static void
set_optionmenu_from_config (GtkWidget *optionmenu, const char *config_name, GList *senum)
{
	gtk_option_menu_set_history (GTK_OPTION_MENU (optionmenu),
				     get_index (config_name, senum));
}


static void
set_radiobuttongroup_from_config (GtkWidget *radiobutton, const char *config_name, GList *senum)
{
	GtkToggleButton *button;
	GSList *list;
	gint length;
	int index;

	index = get_index (config_name, senum);

	/* get the list */
        list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));

	/* check out the length */
        length = g_slist_length (list);

        /* new buttons are *preppended* to the list, so button added as first
         * has last position in the list */
        index = (length - 1) - index;

	/* find the right button */
        button = GTK_TOGGLE_BUTTON (g_slist_nth_data (list, index));

	/* set it... this will de-activate the others in the group */
	if (gtk_toggle_button_get_active (button) == FALSE)
	{
		gtk_toggle_button_set_active (button, TRUE);
	}
}

static void
set_spin_button_from_config (GtkWidget *spinbutton, const char *config_name)
{
	gdouble value;
	gint use_int;

	use_int = (gtk_spin_button_get_digits (GTK_SPIN_BUTTON(spinbutton)) == 0);

	if (use_int)
	{
		/* get the current value from the configuration space */
		value = eel_gconf_get_integer (config_name);
	}
	else
	{
		/* get the current value from the configuration space */
		value = eel_gconf_get_float (config_name);
	}

	/* set this option value in the widget */
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton), value);
}

static void
set_togglebutton_from_config (GtkWidget *togglebutton, const char *config_name)
{
	gboolean value;

	/* get the current value from the configuration space */
	value = eel_gconf_get_boolean (config_name);

	/* set this option value in the widget */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (togglebutton), value);
}

static void
set_color_from_config (GtkWidget *colorpicker, const char *config_name)
{
	gchar *color_string;
	guint r, g, b;

	/* get the string from config */
	color_string = eel_gconf_get_string (config_name);

	if (color_string)
	{
		/* parse it and setup the color picker */
		sscanf (color_string, "#%2X%2X%2X", &r, &g, &b);
		gnome_color_picker_set_i8 (GNOME_COLOR_PICKER (colorpicker),
					   r, g, b, 0);
		/* free the string */
		g_free (color_string);
	}
}

static PrefType
get_pref_type_from_widget (GtkWidget *widget)
{
	if (GTK_IS_OPTION_MENU (widget))
	{
		return PT_OPTIONMENU;
	}
	else if (GTK_IS_SPIN_BUTTON (widget))
	{
		return PT_SPINBUTTON;
	}
	else if (GTK_IS_RADIO_BUTTON (widget))
	{
		return PT_RADIOBUTTON;
	}
	else if (GTK_IS_TOGGLE_BUTTON (widget))
	{
		return PT_TOGGLEBUTTON;
	}
	else if (GTK_IS_ENTRY (widget))
	{
		return PT_ENTRY;
	}
	else if (GNOME_IS_COLOR_PICKER (widget))
	{
		return PT_COLOR;
	}

	return PT_UNKNOWN;
}

static int *
set_controls_sensitivity (EphyDialog *dialog,
			  int *sg, gboolean s)
{
	GtkWidget *widget;

	while (*sg != SY_END_GROUP)
	{
		widget = ephy_dialog_get_control (dialog,
						    *sg);
		gtk_widget_set_sensitive (widget, s);

		sg++;
	}

	return sg;
}

static void
prefs_set_group_sensitivity (GtkWidget *widget,
			     PrefType type, int *sg)
{
	int group = -1;
	EphyDialog *dialog;
	int i = 0;

	if (sg == NULL) return;

	dialog = EPHY_DIALOG (g_object_get_data
				(G_OBJECT(widget), "dialog"));

	if (GTK_IS_RADIO_BUTTON (widget))
	{
		group = get_radio_button_active_index (widget);
	}
	else if (GTK_IS_TOGGLE_BUTTON (widget))
	{
		group = !gtk_toggle_button_get_active
			(GTK_TOGGLE_BUTTON(widget));
	}
	else
	{
		g_assert (FALSE);
	}

	while (*sg != SY_END)
	{
		if ((*sg == SY_BEGIN_GROUP) ||
		    (*sg == SY_BEGIN_GROUP_INVERSE))
		{
			gboolean b;

			b = (i == group);
			if (*sg == SY_BEGIN_GROUP_INVERSE) b = !b;

			sg++;
			sg = set_controls_sensitivity
				(dialog, sg, b);
		}

		i++;
		sg++;
	}
}

static void
prefs_togglebutton_clicked_cb (GtkWidget *widget, PropertyInfo *pi)
{
	if (pi->type == PT_AUTOAPPLY)
	{
		set_config_from_togglebutton (widget, pi->pref);
	}

	prefs_set_group_sensitivity (widget, pi->type, pi->sg);
}

static void
prefs_radiobutton_clicked_cb (GtkWidget *widget, PropertyInfo *pi)
{
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget)))
	{
		return;
	}

	if (pi->type == PT_AUTOAPPLY)
	{
		set_config_from_radiobuttongroup (widget, pi->pref, pi->string_enum);
	}

	prefs_set_group_sensitivity (widget, pi->type, pi->sg);
}

static gint
prefs_spinbutton_timeout_cb (EphyDialog *dialog)
{
	PropertyInfo pi = dialog->priv->props[dialog->priv->spin_item_id];

        /* timer still valid? */
        if (dialog->priv->spin_timer == NULL)
        {
                return FALSE;
        }

        /* okay, we're ready to set */
        if (g_timer_elapsed (dialog->priv->spin_timer, NULL) >= SPIN_DELAY)
        {
                /* kill off the timer */
                g_timer_destroy (dialog->priv->spin_timer);
                dialog->priv->spin_timer = NULL;

		/* HACK update the spinbutton here so that the
		 * changes made directly in the entry are accepted
		 * and set in the pref. Otherwise the old value is used */
		gtk_spin_button_update (GTK_SPIN_BUTTON(pi.widget));

		/* set */
                set_config_from_spin_button (pi.widget, pi.pref);

                /* done now */
                return FALSE;
        }

        /* call me again */
        return TRUE;
}

static void
prefs_spinbutton_changed_cb (GtkWidget *widget, PropertyInfo *pi)
{
	EphyDialog *dialog;

	if (pi->type != PT_AUTOAPPLY) return;

	dialog = EPHY_DIALOG (g_object_get_data
				(G_OBJECT(widget), "dialog"));

	dialog->priv->spin_item_id = pi->id;

	/* destroy any existing timer */
	if (dialog->priv->spin_timer != NULL)
	{
		g_timer_destroy (dialog->priv->spin_timer);
	}

	/* start the new one */
	dialog->priv->spin_timer = g_timer_new();
	g_timer_start (dialog->priv->spin_timer);
	g_timeout_add (50, (GSourceFunc) prefs_spinbutton_timeout_cb,
		       dialog);
}

static void
prefs_color_changed_cb (GtkWidget *widget, guint r, guint g,
			guint b, guint a, const PropertyInfo *pi)
{
	if (pi->type == PT_AUTOAPPLY)
	{
		set_config_from_color (widget,  pi->pref);
	}
}

static void
prefs_entry_changed_cb (GtkWidget *widget, PropertyInfo *pi)
{
	if (pi->type == PT_AUTOAPPLY)
	{
		set_config_from_editable (widget,  pi->pref);
	}
}

static void
prefs_optionmenu_selected_cb (GtkWidget *widget, PropertyInfo *pi)
{
	if (pi->type == PT_AUTOAPPLY)
	{
		set_config_from_optionmenu (widget, pi->pref, pi->string_enum);
	}
}

static void
prefs_connect_signals (EphyDialog *dialog)
{
	int i;
	GSList *list;
	PropertyInfo *props = dialog->priv->props;

	for (i = 0; props[i].widget != NULL; i++)
	{
		PrefType type;
		PropertyInfo *info;

		if ((props[i].type != PT_AUTOAPPLY) &&
		    (props[i].sg == NULL))
			continue;

		info = &dialog->priv->props[i];
		type = get_pref_type_from_widget
			(dialog->priv->props[i].widget);

		switch (type)
		{
		case PT_TOGGLEBUTTON:
			g_object_set_data (G_OBJECT(info->widget), "dialog", dialog);
			g_signal_connect (G_OBJECT (info->widget), "clicked",
					  G_CALLBACK(prefs_togglebutton_clicked_cb),
					  (gpointer)info);
			break;
		case PT_RADIOBUTTON:
			list = gtk_radio_button_get_group
				(GTK_RADIO_BUTTON(info->widget));
			for (; list != NULL; list = list->next)
			{
				g_object_set_data (G_OBJECT(list->data),
						   "dialog", dialog);
				g_signal_connect
					(G_OBJECT (list->data), "clicked",
					 G_CALLBACK(prefs_radiobutton_clicked_cb),
					 (gpointer)info);
			}
			break;
		case PT_SPINBUTTON:
			g_object_set_data (G_OBJECT(info->widget), "dialog", dialog);
			g_signal_connect (G_OBJECT (info->widget), "changed",
					  G_CALLBACK(prefs_spinbutton_changed_cb),
					  (gpointer)info);
			break;
		case PT_COLOR:
			g_signal_connect (G_OBJECT (info->widget), "color_set",
					  G_CALLBACK(prefs_color_changed_cb),
					  (gpointer)info);
			break;
		case PT_OPTIONMENU:
			g_signal_connect (G_OBJECT (info->widget),
					  "changed",
					  G_CALLBACK(prefs_optionmenu_selected_cb),
					  (gpointer)info);
			break;
		case PT_ENTRY:
			g_signal_connect (G_OBJECT (info->widget), "changed",
					  G_CALLBACK(prefs_entry_changed_cb),
					  (gpointer)info);
			break;
		case PT_UNKNOWN:
			break;
		}
	}
}

static void
ephy_dialog_init (EphyDialog *dialog)
{
        dialog->priv = g_new0 (EphyDialogPrivate, 1);

	dialog->priv->parent = NULL;
	dialog->priv->dialog = NULL;
	dialog->priv->props = NULL;
	dialog->priv->spin_timer = NULL;
	dialog->priv->name = NULL;
	dialog->priv->initialized = FALSE;
	dialog->priv->has_default_size = FALSE;
	dialog->priv->disposing = FALSE;
}

static void
prefs_set_sensitivity (PropertyInfo *props)
{
	int i;

	for (i=0 ; props[i].id >= 0; i++)
	{
		if (props[i].sg == NULL) continue;

		g_return_if_fail (props[i].widget != NULL);

		if (GTK_IS_RADIO_BUTTON(props[i].widget) ||
		    GTK_IS_TOGGLE_BUTTON(props[i].widget))
		{
			prefs_set_group_sensitivity (props[i].widget,
						     props[i].type,
						     props[i].sg);
		}
	}
}

static void
load_props (PropertyInfo *props)
{
	int i;

	for (i=0 ; props[i].id >= 0; i++)
	{
		if (props[i].pref == NULL) continue;

		g_return_if_fail (props[i].widget != NULL);

		if (GTK_IS_SPIN_BUTTON(props[i].widget))
		{
			set_spin_button_from_config (props[i].widget,
						     props[i].pref);
		}
		else if (GTK_IS_RADIO_BUTTON(props[i].widget))
		{
			set_radiobuttongroup_from_config (props[i].widget,
						          props[i].pref,
							  props[i].string_enum);
		}
		else if (GTK_IS_TOGGLE_BUTTON(props[i].widget))
		{
			 set_togglebutton_from_config (props[i].widget,
						       props[i].pref);
		}
		else if (GTK_IS_EDITABLE(props[i].widget))
		{
			set_editable_from_config (props[i].widget,
						  props[i].pref);
		}
		else if (GTK_IS_OPTION_MENU(props[i].widget))
		{
			set_optionmenu_from_config (props[i].widget,
						    props[i].pref,
						    props[i].string_enum);
		}
		else if (GNOME_IS_COLOR_PICKER(props[i].widget))
		{
			set_color_from_config (props[i].widget,
					       props[i].pref);
		}
	}

}

static void
save_props (PropertyInfo *props)
{
	int i;

	for (i=0 ; props[i].id >= 0; i++)
	{
		if ((props[i].pref == NULL) ||
		    (props[i].type != PT_NORMAL)) continue;
		g_return_if_fail (props[i].widget != NULL);

		if (GTK_IS_SPIN_BUTTON(props[i].widget))
		{
			set_config_from_spin_button (props[i].widget,
						     props[i].pref);
		}
		else if (GTK_IS_RADIO_BUTTON(props[i].widget))
		{
			set_config_from_radiobuttongroup (props[i].widget,
							  props[i].pref,
							  props[i].string_enum);
		}
		else if (GTK_IS_TOGGLE_BUTTON(props[i].widget))
		{
			 set_config_from_togglebutton (props[i].widget,
						       props[i].pref);
		}
		else if (GTK_IS_EDITABLE(props[i].widget))
		{
			set_config_from_editable (props[i].widget,
						  props[i].pref);
		}
		else if (GTK_IS_OPTION_MENU(props[i].widget))
		{
			set_config_from_optionmenu (props[i].widget,
						    props[i].pref,
						    props[i].string_enum);
		}
		else if (GNOME_IS_COLOR_PICKER(props[i].widget))
		{
			set_config_from_color (props[i].widget,
					       props[i].pref);
		}
	}
}

static void
free_props (PropertyInfo *properties)
{
	int i;

	for (i = 0; properties[i].string_enum != NULL; i++)
	{
		g_list_foreach (properties[i].string_enum, (GFunc)g_free, NULL);
		g_list_free (properties[i].string_enum);
	}
}

static void
ephy_dialog_finalize (GObject *object)
{
        EphyDialog *dialog;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_EPHY_DIALOG (object));

        dialog = EPHY_DIALOG (object);

        g_return_if_fail (dialog->priv != NULL);

	free_props (dialog->priv->props);

	g_free (dialog->priv->name);
	g_free (dialog->priv->props);
        g_free (dialog->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_dialog_set_property (GObject *object,
                          guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
        EphyDialog *d = EPHY_DIALOG (object);

        switch (prop_id)
        {
                case PROP_PARENT_WINDOW:
                        ephy_dialog_set_parent (d, g_value_get_object (value));
                        break;
		case PROP_MODAL:
			ephy_dialog_set_modal (d, g_value_get_boolean (value));
			break;
        }
}

static void
ephy_dialog_get_property (GObject *object,
			  guint prop_id,
                          GValue *value,
                          GParamSpec *pspec)
{
        EphyDialog *d = EPHY_DIALOG (object);

        switch (prop_id)
        {
                case PROP_PARENT_WINDOW:
                        g_value_set_object (value, d->priv->parent);
                        break;
		case PROP_MODAL:
			g_value_set_boolean (value, d->priv->modal);
        }
}

static void
ephy_dialog_set_parent (EphyDialog *dialog,
			  GtkWidget *parent)
{
	g_return_if_fail (dialog->priv->parent == NULL);
        g_return_if_fail (GTK_IS_WINDOW(parent));
	g_return_if_fail (GTK_IS_WINDOW(dialog->priv->dialog));

        dialog->priv->parent = parent;

	gtk_window_set_transient_for (GTK_WINDOW (dialog->priv->dialog),
                                      GTK_WINDOW (parent));
}

EphyDialog *
ephy_dialog_new (void)
{
	return EPHY_DIALOG (g_object_new (EPHY_DIALOG_TYPE, NULL));
}

EphyDialog *
ephy_dialog_new_with_parent (GtkWidget *parent_window)
{
	return EPHY_DIALOG (g_object_new (EPHY_DIALOG_TYPE,
					    "ParentWindow", parent_window,
					    NULL));
}

void
ephy_dialog_add_enum (EphyDialog *dialog,
		      int id,
		      guint n_items,
		      const char **items)
{
	int i = 0;
	GList *l = NULL;

	for (i = 0; i < n_items; i++)
	{
		l = g_list_append (l, g_strdup (items[i]));
	}

	dialog->priv->props[id].string_enum = l;
}

static PropertyInfo *
init_props (const EphyDialogProperty *properties, GladeXML *gxml)
{
	PropertyInfo *props;
	int i;

	for (i=0 ; properties[i].control_name != NULL; i++);

	props = g_new0 (PropertyInfo, i+1);

	for (i=0 ; properties[i].control_name != NULL; i++)
	{
		props[i].id = properties[i].id;
		props[i].widget = glade_xml_get_widget
					(gxml, properties[i].control_name);
		props[i].pref = properties[i].state_pref;
		props[i].sg = properties[i].sg;
		props[i].type = properties[i].type;
		props[i].string_enum = NULL;
	}

	props[i].id = -1;
	props[i].widget = NULL;
	props[i].pref = NULL;
	props[i].sg = NULL;
	props[i].type = 0;
	props[i].string_enum = NULL;

	return props;
}

static void
dialog_destroy_cb (GtkWidget *widget, EphyDialog *dialog)
{
	if (dialog->priv->props)
	{
		save_props (dialog->priv->props);
	}

	if (!dialog->priv->disposing)
	{
		g_object_unref (dialog);
	}
}

static void
impl_construct (EphyDialog *dialog,
		const EphyDialogProperty *properties,
		const char *file,
		const char *name)
{
	GladeXML *gxml;

	gxml = ephy_glade_widget_new
		(file, name, &(dialog->priv->dialog), dialog);

	if (dialog->priv->name == NULL)
	{
		dialog->priv->name = g_strdup (name);
	}

	if (properties)
	{
		dialog->priv->props = init_props (properties, gxml);
	}

	g_signal_connect_object (dialog->priv->dialog,
			         "destroy",
			         G_CALLBACK(dialog_destroy_cb),
			         dialog, 0);

	g_object_unref (gxml);
}

static GtkWidget *
impl_get_control (EphyDialog *dialog,
                  int property_id)
{
	return dialog->priv->props[property_id].widget;
}

static void
impl_get_value (EphyDialog *dialog,
                int property_id,
                GValue *value)
{
	GtkWidget *widget = ephy_dialog_get_control (dialog,
						       property_id);

	if (GTK_IS_SPIN_BUTTON (widget))
	{
		float val;
		g_value_init (value, G_TYPE_FLOAT);
		val = gtk_spin_button_get_value (GTK_SPIN_BUTTON(widget));
		g_value_set_float (value, val);
	}
	else if (GTK_IS_RADIO_BUTTON (widget))
	{
		int val;
		g_value_init (value, G_TYPE_INT);
		val = get_radio_button_active_index (widget);
		g_value_set_int (value, val);
	}
	else if (GTK_IS_TOGGLE_BUTTON (widget))
	{
		g_value_init (value, G_TYPE_BOOLEAN);
		g_value_set_boolean (value, gtk_toggle_button_get_active
		                             (GTK_TOGGLE_BUTTON(widget)));
	}
	else if (GTK_IS_EDITABLE (widget))
	{
		gchar *text = gtk_editable_get_chars (GTK_EDITABLE (widget), 0, -1);
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, text);
		g_free (text);
	}
	else if (GTK_IS_OPTION_MENU (widget))
	{
		int val;
		g_value_init (value, G_TYPE_INT);
		val = gtk_option_menu_get_history (GTK_OPTION_MENU(widget));
		g_value_set_int (value, val);
	}
}

static void
setup_default_size (EphyDialog *dialog)
{
	if (!dialog->priv->has_default_size)
	{
		ephy_state_add_window (dialog->priv->dialog,
				       dialog->priv->name, -1, -1,
				       EPHY_STATE_WINDOW_SAVE_SIZE);
	}

	dialog->priv->has_default_size = TRUE;
}

static gint
impl_run (EphyDialog *dialog)
{
	setup_default_size (dialog);

	return gtk_dialog_run (GTK_DIALOG(dialog->priv->dialog));
}

static void
impl_show (EphyDialog *dialog)
{
	if (dialog->priv->props && !dialog->priv->initialized)
	{
		load_props (dialog->priv->props);
		prefs_connect_signals (dialog);
		prefs_set_sensitivity (dialog->priv->props);
		dialog->priv->initialized = TRUE;
	}

	setup_default_size (dialog);

	if (dialog->priv->parent)
	{
		/* make the dialog transient again, because it seems to get
		 * forgotten after gtk_widget_hide
		 */
		gtk_window_set_transient_for (GTK_WINDOW (dialog->priv->dialog),
					      GTK_WINDOW (dialog->priv->parent));
	}
	gtk_window_present (GTK_WINDOW(dialog->priv->dialog));
}

void
ephy_dialog_set_modal (EphyDialog *dialog,
		       gboolean is_modal)
{
	dialog->priv->modal = is_modal;

	gtk_window_set_modal (GTK_WINDOW(dialog->priv->dialog),
			      is_modal);
}

void
ephy_dialog_construct (EphyDialog *dialog,
			 const EphyDialogProperty *properties,
			 const char *file,
			 const char *name)
{
	EphyDialogClass *klass = EPHY_DIALOG_GET_CLASS (dialog);
        return klass->construct (dialog, properties, file, name);
}

gint
ephy_dialog_run (EphyDialog *dialog)
{
	EphyDialogClass *klass = EPHY_DIALOG_GET_CLASS (dialog);
        return klass->run (dialog);
}

void
ephy_dialog_show (EphyDialog *dialog)
{
	EphyDialogClass *klass = EPHY_DIALOG_GET_CLASS (dialog);
        klass->show (dialog);
}

GtkWidget *
ephy_dialog_get_control (EphyDialog *dialog,
			 int property_id)
{
	EphyDialogClass *klass = EPHY_DIALOG_GET_CLASS (dialog);
        return klass->get_control (dialog, property_id);
}

void
ephy_dialog_get_value (EphyDialog *dialog,
		       int property_id,
		       GValue *value)
{
	EphyDialogClass *klass = EPHY_DIALOG_GET_CLASS (dialog);
        return klass->get_value (dialog, property_id, value);
}

void
ephy_dialog_set_size_group (EphyDialog *dialog,
			    int *controls_id,
			    guint n_controls)
{
	GtkSizeGroup *size_group;
	int i;

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	for (i = 0; i < n_controls; i++)
	{
		GtkWidget *widget;
		guint id;

		id = controls_id[i];
		widget = dialog->priv->props[id].widget;
		g_return_if_fail (GTK_IS_WIDGET (widget));

                gtk_size_group_add_widget (size_group, widget);
        }
}
