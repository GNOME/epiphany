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
 */

#include "config.h"

#include "ephy-dialog.h"
#include "ephy-initial-state.h"
#include "ephy-gui.h"
#include "ephy-debug.h"

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

/**
 * SECTION:ephy-dialog
 * @short_description: A customized #GtkDialog for Epiphany
 *
 * A customized #GtkDialog for Epiphany.
 */

enum
{
	PROP_0,
	PROP_PARENT_WINDOW,
	PROP_PERSIST_POSITION,
	PROP_DEFAULT_WIDTH,
	PROP_DEFAULT_HEIGHT
};

#define EPHY_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_DIALOG, EphyDialogPrivate))

struct _EphyDialogPrivate
{
	char *name;

	GtkWidget *parent;
	GtkWidget *dialog;

	GtkBuilder *builder;

	guint has_default_size : 1;
	guint disposing : 1;
	guint initialized : 1;
	guint persist_position : 1;
	int default_width;
	int default_height;
};

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

static void
setup_default_size (EphyDialog *dialog)
{
	if (dialog->priv->has_default_size == FALSE)
	{
		EphyInitialStateWindowFlags flags;

		flags = EPHY_INITIAL_STATE_WINDOW_SAVE_SIZE;

		if (dialog->priv->persist_position)
		{
			flags |= EPHY_INITIAL_STATE_WINDOW_SAVE_POSITION;
		}

		ephy_initial_state_add_window (dialog->priv->dialog,
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
	if (dialog->priv->disposing == FALSE)
	{
		g_object_unref (dialog);
	}
}

static void
impl_construct (EphyDialog *dialog,
		const char *resource,
		const char *name,
		const char *domain)
{
	EphyDialogPrivate *priv = dialog->priv;
	GtkBuilder *builder;
	GError *error = NULL;

	builder = gtk_builder_new ();
	gtk_builder_set_translation_domain (builder, domain);

	/* Hack to support extensions that use EphyDialog with files and
	 * not GResource objects. This is far simpler than creating a
	 * GResource binary for every extension. */
	if (g_file_test (resource, G_FILE_TEST_EXISTS))
	{
		gtk_builder_add_from_file (builder, resource, &error);
	}
	else
	{
		gtk_builder_add_from_resource (builder, resource, &error);
	}

	if (error)
	{
		g_warning ("Unable to load UI resource %s: %s", resource, error->message);
		g_error_free (error);
		return;
	}

	priv->builder = g_object_ref (builder);
	priv->dialog = GTK_WIDGET (gtk_builder_get_object (builder, name));

	g_return_if_fail (priv->dialog != NULL);

	if (priv->name == NULL)
	{
		priv->name = g_strdup (name);
	}

	g_signal_connect_object (dialog->priv->dialog, "destroy",
				 G_CALLBACK(dialog_destroy_cb), dialog, 0);

	g_object_unref (builder);
}

static void
impl_show (EphyDialog *dialog)
{
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

/**
 * ephy_dialog_set_size_group:
 * @dialog: an #EphyDialog
 * @first_id: id of a widget in @dialog
 * @Varargs: a %NULL-terminated list of widget ids
 *
 * Put @first_id and @Varargs widgets into the same #GtkSizeGroup.
 * Note that this are all widgets inside @dialog.
 **/
void
ephy_dialog_set_size_group (EphyDialog *dialog,
			    const char *first_id,
			    ...)
{
	GtkSizeGroup *size_group;
	va_list vl;

	g_return_if_fail (EPHY_IS_DIALOG (dialog));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	va_start (vl, first_id);

	while (first_id != NULL)
	{
		GtkWidget *widget;

		widget = ephy_dialog_get_control (dialog, first_id);
		g_return_if_fail (widget != NULL);

		gtk_size_group_add_widget (size_group, widget);

		first_id = va_arg (vl, const char*);
	}

	va_end (vl);

	g_object_unref (size_group);
}

/**
 * ephy_dialog_construct:
 * @dialog: an #EphyDialog
 * @resource: the path to the UI resource
 * @name: name of the widget to use for @dialog, found in @file
 * @domain: translation domain to set for @dialog
 *
 * Constructs the widget part of @dialog using the widget identified by @name
 * in the #GtkBuilder file found at @file.
 **/
void
ephy_dialog_construct (EphyDialog *dialog,
		       const char *resource,
		       const char *name,
		       const char *domain)
{
	EphyDialogClass *klass = EPHY_DIALOG_GET_CLASS (dialog);
	klass->construct (dialog, resource, name, domain);
}

/**
 * ephy_dialog_show:
 * @dialog: an #EphyDialog
 *
 * Shows @dialog on screen.
 **/
void
ephy_dialog_show (EphyDialog *dialog)
{
	EphyDialogClass *klass;

	g_return_if_fail (EPHY_IS_DIALOG (dialog));

	klass = EPHY_DIALOG_GET_CLASS (dialog);
	klass->show (dialog);
}

/**
 * ephy_dialog_hide:
 * @dialog: an #EphyDialog
 *
 * Calls gtk_widget_hide on @dialog.
 **/
void
ephy_dialog_hide (EphyDialog *dialog)
{
	g_return_if_fail (EPHY_IS_DIALOG (dialog));
	g_return_if_fail (dialog->priv->dialog != NULL);

	gtk_widget_hide (dialog->priv->dialog);
}

/**
 * ephy_dialog_run:
 * @dialog: an #EphyDialog
 *
 * Runs gtk_dialog_run on @dialog and waits for a response.
 *
 * Returns: the user response to gtk_dialog_run or 0 if @dialog is not valid
 **/
int
ephy_dialog_run (EphyDialog *dialog)
{
	g_return_val_if_fail (EPHY_IS_DIALOG (dialog), 0);

	ephy_dialog_show (dialog);

	gtk_window_group_add_window (ephy_gui_ensure_window_group (GTK_WINDOW (dialog->priv->parent)),
				     GTK_WINDOW (dialog->priv->dialog));

	return gtk_dialog_run (GTK_DIALOG (dialog->priv->dialog));
}

/**
 * ephy_dialog_get_control:
 * @dialog: an #EphyDialog
 * @property_id: the string identifier of the requested control
 *
 * Gets the internal widget corresponding to @property_id from @dialog.
 * Return value: (transfer none): the #GtkWidget corresponding to @property_id
 * or %NULL
 **/
GtkWidget *
ephy_dialog_get_control (EphyDialog *dialog,
			 const char *object_id)
{
	GtkWidget *widget;

	g_return_val_if_fail (EPHY_IS_DIALOG (dialog), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (dialog->priv->builder,
						     object_id));

	return widget;
}

/**
 * ephy_dialog_get_controls:
 * @dialog: an #EphyDialog
 * @first_id: identifier of the requested control
 * @Varargs: a %NULL terminated list of extra pairs of properties as const char
 * and store locations as #GtkWidgets.
 *
 * Gets the requested controls according to given property-store_location pairs.
 * Properties are given as strings (const char *), controls are returned as
 * #GtkWidget elements.
 * Rename to: ephy_dialog_get_controls
 **/
void
ephy_dialog_get_controls (EphyDialog *dialog,
			  const char *first_id,
			  ...)
{
	GtkWidget **wptr;
	va_list varargs;

	va_start (varargs, first_id);

	while (first_id != NULL)
	{
		wptr = va_arg (varargs, GtkWidget **);
		*wptr = ephy_dialog_get_control (dialog, first_id);

		first_id = va_arg (varargs, const char *);
	}

	va_end (varargs);
}

static void
ephy_dialog_init (EphyDialog *dialog)
{
	dialog->priv = EPHY_DIALOG_GET_PRIVATE (dialog);

	dialog->priv->default_width = -1;
	dialog->priv->default_height = -1;
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

	g_clear_object (&dialog->priv->builder);

	parent_class->dispose (object);
}

static void
ephy_dialog_finalize (GObject *object)
{
	EphyDialog *dialog = EPHY_DIALOG (object);

	g_free (dialog->priv->name);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * ephy_dialog_get_parent:
 * @dialog: an #EphyDialog
 *
 * Gets @dialog's parent-window.
 *
 * Returns: (transfer none): the parent-window of @dialog
 **/
GtkWidget *
ephy_dialog_get_parent (EphyDialog *dialog)
{
	g_return_val_if_fail (EPHY_IS_DIALOG (dialog), NULL);

	return dialog->priv->parent;
}

/**
 * ephy_dialog_set_parent:
 * @dialog: an #EphyDialog
 * @parent: new parent for @dialog
 *
 * Sets @parent as the parent-window of @dialog.
 **/
void
ephy_dialog_set_parent (EphyDialog *dialog,
			GtkWidget *parent)
{
	g_return_if_fail (EPHY_IS_DIALOG (dialog));

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

	/**
	* EphyDialog::changed:
	* @dialog: the object on which the signal is emitted
	* @value: new value of the modified widget, as a #GValue
	*
	* Emitted everytime a child widget of the dialog has its changed or
	* clicked signal emitted.
	*/
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

	/**
	* EphyDialog:parent-window:
	*
	* Dialog's parent window.
	*/
	g_object_class_install_property (object_class,
					 PROP_PARENT_WINDOW,
					 g_param_spec_object ("parent-window",
							      "Parent window",
							      "Parent window",
							      GTK_TYPE_WINDOW,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	/**
	* EphyDialog:persist-position:
	*
	* If dialog position should be persistent.
	*/
	g_object_class_install_property (object_class,
					 PROP_PERSIST_POSITION,
					 g_param_spec_boolean ("persist-position",
							       "Persist position",
							       "Persist dialog position",
							       FALSE,
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							       G_PARAM_CONSTRUCT_ONLY));

	/**
	* EphyDialog:default-width:
	*
	* The dialog default width.
	*/
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

	/**
	* EphyDialog:default-height:
	*
	* The dialog default height.
	*/
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

/**
 * ephy_dialog_new:
 *
 * Creates a new #EphyDialog.
 *
 * Returns: a new #EphyDialog
 **/
EphyDialog *
ephy_dialog_new (void)
{
	return EPHY_DIALOG (g_object_new (EPHY_TYPE_DIALOG, NULL));
}

/**
 * ephy_dialog_new_with_parent:
 * @parent_window: a window to be parent of the new dialog
 *
 * Creates a new #EphyDialog with @parent_window as its parent.
 *
 * Returns: a new #EphyDialog
 **/
EphyDialog *
ephy_dialog_new_with_parent (GtkWidget *parent_window)
{
	return EPHY_DIALOG (g_object_new (EPHY_TYPE_DIALOG,
					  "parent-window", parent_window,
					  NULL));
}
