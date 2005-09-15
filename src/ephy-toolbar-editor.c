/* 
 *  Copyright (C) 2000-2004 Marco Pesenti Gritti
 *  Copyright (C) 2003-2005 Christian Persch
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

#include "config.h"

#include "ephy-toolbar-editor.h"
#include "ephy-gui.h"
#include "ephy-prefs.h"
#include "ephy-state.h"
#include "ephy-file-helpers.h"
#include "ephy-shell.h"
#include "eggtypebuiltins.h"
#include "egg-toolbars-model.h"
#include "egg-editable-toolbar.h"
#include "egg-toolbar-editor.h"
#include "eel-gconf-extensions.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcelllayout.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtkstock.h>
#include <string.h>

#define DATA_KEY		"EphyToolbarEditor"
#define CONTROL_CENTRE_DOMAIN	"control-center-2.0"

#define EPHY_TOOLBAR_EDITOR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TOOLBAR_EDITOR, EphyToolbarEditorPrivate))

struct _EphyToolbarEditorPrivate
{
	EggToolbarsModel *model;
	EphyWindow *window;
};

enum
{
	RESPONSE_ADD_TOOLBAR = 1
};

enum
{
	COL_TEXT,
	COL_FLAGS,
	COL_IS_SEP
};

static const struct
{
	const char *text;
	EggTbModelFlags flags;
	gboolean cc_domain;
}
toolbar_styles [] =
{
	{ /* Translators: The text before the "|" is context to help you decide on
	   * the correct translation. You MUST OMIT it in the translated string. */
	  N_("toolbar style|Default"), 0, FALSE },
	{ NULL /* separator row */, 0, FALSE },
	{ "Text below icons", EGG_TB_MODEL_BOTH, TRUE },
	{ "Text beside icons", EGG_TB_MODEL_BOTH_HORIZ, TRUE },
	{ "Icons only", EGG_TB_MODEL_ICONS, TRUE },
	{ "Text only", EGG_TB_MODEL_TEXT, TRUE }
};

enum
{
	PROP_0,
	PROP_WINDOW
};

static GObjectClass *parent_class = NULL;

static gboolean
row_is_separator (GtkTreeModel *model,
		  GtkTreeIter *iter,
		  gpointer data)
{
	gboolean is_sep;
	gtk_tree_model_get (model, iter, COL_IS_SEP, &is_sep, -1);
	return is_sep;
}

static void
combo_changed_cb (GtkComboBox *combo,
		  GtkListStore *store)
{
	GtkTreeIter iter;
	EggTbModelFlags flags;
	GFlagsClass *flags_class;
	const GFlagsValue *value;
	const char *pref = "";

	if  (!gtk_combo_box_get_active_iter (combo, &iter)) return;

	gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, COL_FLAGS, &flags, -1);

	flags_class = g_type_class_ref (EGG_TYPE_TB_MODEL_FLAGS);
	value = g_flags_get_first_value (flags_class, flags);
	if (value != NULL)
	{
		pref = value->value_nick;
	}

	eel_gconf_set_string (CONF_INTERFACE_TOOLBAR_STYLE, pref);

	g_type_class_unref (flags_class);
}


static void
ephy_toolbar_editor_response (GtkDialog *dialog,
			      gint response_id)
{
	EphyToolbarEditorPrivate *priv = EPHY_TOOLBAR_EDITOR (dialog)->priv;

	switch (response_id)
	{
		case GTK_RESPONSE_CLOSE:
			gtk_widget_destroy (GTK_WIDGET (dialog));
			break;
		case RESPONSE_ADD_TOOLBAR:
			egg_toolbars_model_add_toolbar (priv->model, -1, "UserCreated");
			break;
		case GTK_RESPONSE_HELP:
			ephy_gui_help (GTK_WINDOW (dialog), "epiphany", "to-edit-toolbars");
			break;
	}
}

static void
ephy_toolbar_editor_init (EphyToolbarEditor *editor)
{
	editor->priv = EPHY_TOOLBAR_EDITOR_GET_PRIVATE (editor);
	
	editor->priv->model = EGG_TOOLBARS_MODEL
		(ephy_shell_get_toolbars_model (ephy_shell, FALSE));
}

static GObject *
ephy_toolbar_editor_constructor (GType type,
				 guint n_construct_properties,
				 GObjectConstructParam *construct_params)

{
        GObject *object;
	EphyToolbarEditorPrivate *priv;
	GtkWidget *dialog, *editor, *toolbar, *vbox, *hbox, *label, *combo;
	GtkUIManager *manager;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkCellRenderer *renderer;
	GFlagsClass *flags_class;
	const GFlagsValue *value;
	EggTbModelFlags flags = 0;
	char *pref;
	int i;

        object = parent_class->constructor (type, n_construct_properties,
					    construct_params);

#ifdef ENABLE_NLS
        /* Initialize the control centre domain */
        bindtextdomain (CONTROL_CENTRE_DOMAIN, GNOMELOCALEDIR);
        bind_textdomain_codeset(CONTROL_CENTRE_DOMAIN, "UTF-8");
#endif

	dialog = GTK_WIDGET (object);
	priv = EPHY_TOOLBAR_EDITOR (object)->priv;

	toolbar = ephy_window_get_toolbar (priv->window);

	vbox = GTK_DIALOG (dialog)->vbox;
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Toolbar Editor"));
        gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (priv->window));
	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");

	manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (priv->window));
	editor = egg_toolbar_editor_new (manager, priv->model);
	egg_toolbar_editor_load_actions (EGG_TOOLBAR_EDITOR (editor),
					 ephy_file ("epiphany-toolbar.xml"));
	gtk_container_set_border_width (GTK_CONTAINER (EGG_TOOLBAR_EDITOR (editor)), 5);
	gtk_box_set_spacing (GTK_BOX (EGG_TOOLBAR_EDITOR (editor)), 5);
	gtk_box_pack_start (GTK_BOX (vbox), editor, TRUE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_set_spacing (GTK_BOX (editor), 18);
	gtk_box_pack_start (GTK_BOX (editor), hbox, FALSE, FALSE, 0);

	/* translators: translate the same as in gnome-control-center */
	label = gtk_label_new_with_mnemonic (_("Toolbar _button labels:"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

	store = gtk_list_store_new (3, G_TYPE_STRING, EGG_TYPE_TB_MODEL_FLAGS,
				    G_TYPE_BOOLEAN);

	for (i = 0; i < G_N_ELEMENTS (toolbar_styles); i++)
	{
		const char *text = toolbar_styles[i].text;
		const char *tr_text = NULL;

		if (toolbar_styles[i].cc_domain)
		{
			tr_text = dgettext (CONTROL_CENTRE_DOMAIN, text);
		}
		else if (text != NULL)
		{
			tr_text= Q_(text);
		}

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_TEXT, tr_text,
				    COL_FLAGS, toolbar_styles[i].flags,
				    COL_IS_SEP, toolbar_styles[i].text == NULL,
				    -1);
	}

	combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	g_object_unref (store);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
					"text", COL_TEXT, NULL);
	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combo),
					      (GtkTreeViewRowSeparatorFunc) row_is_separator,
					      NULL, NULL);

	gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
	gtk_widget_show_all (hbox);

	/* get active from pref */
	pref = eel_gconf_get_string (CONF_INTERFACE_TOOLBAR_STYLE);
	if (pref != NULL)
	{
		flags_class = g_type_class_ref (EGG_TYPE_TB_MODEL_FLAGS);
		value = g_flags_get_value_by_nick (flags_class, pref);
		if (value != NULL)
		{
			flags = value->value;
		}
		g_type_class_unref (flags_class);
	}
	g_free (pref);

	/* this will set i to 0 if the style is unknown or default */
	for (i = G_N_ELEMENTS (toolbar_styles) - 1; i > 0; i--)
	{
		if (flags & toolbar_styles[i].flags) break;
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), i);
	g_signal_connect (combo, "changed",
			G_CALLBACK (combo_changed_cb), store);

	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("_Add a New Toolbar"), RESPONSE_ADD_TOOLBAR);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       GTK_STOCK_HELP, GTK_RESPONSE_HELP);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

	gtk_widget_show (editor);
	
	ephy_state_add_window (dialog, "toolbar_editor",
		               500, 330, FALSE,
			       EPHY_STATE_WINDOW_SAVE_SIZE);
	gtk_widget_show (dialog);

	egg_editable_toolbar_set_edit_mode (EGG_EDITABLE_TOOLBAR (toolbar), TRUE);
	egg_editable_toolbar_set_edit_mode
		(EGG_EDITABLE_TOOLBAR (ephy_window_get_bookmarksbar (priv->window)), TRUE);

	return object;
}

static void
ephy_toolbar_editor_finalize (GObject *object)
{
	EphyToolbarEditor *editor = EPHY_TOOLBAR_EDITOR (object);
	EphyToolbarEditorPrivate *priv = editor->priv;

	egg_editable_toolbar_set_edit_mode (EGG_EDITABLE_TOOLBAR
		(ephy_window_get_toolbar (priv->window)), FALSE);
	egg_editable_toolbar_set_edit_mode (EGG_EDITABLE_TOOLBAR
		(ephy_window_get_bookmarksbar (priv->window)), FALSE);

	g_object_set_data (G_OBJECT (priv->window), DATA_KEY, NULL);

	parent_class->finalize (object);
}

static void
ephy_toolbar_editor_get_property (GObject *object,
				  guint prop_id,
				  GValue *value,
				  GParamSpec *pspec)
{
	/* no readable properties */
	g_assert_not_reached ();
}

static void
ephy_toolbar_editor_set_property (GObject *object,
				  guint prop_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
        EphyToolbarEditorPrivate *priv = EPHY_TOOLBAR_EDITOR (object)->priv;

        switch (prop_id)
        {
		case PROP_WINDOW:
			priv->window = g_value_get_object (value);
			break;
        }
}

static void
ephy_toolbar_editor_class_init (EphyToolbarEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->constructor = ephy_toolbar_editor_constructor;
	object_class->finalize = ephy_toolbar_editor_finalize;
	object_class->get_property = ephy_toolbar_editor_get_property;
	object_class->set_property = ephy_toolbar_editor_set_property;

	dialog_class->response = ephy_toolbar_editor_response;

	g_object_class_install_property (object_class,
                                         PROP_WINDOW,
                                         g_param_spec_object ("window",
                                                              "Window",
                                                              "Parent window",
							      EPHY_TYPE_WINDOW,
							      G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (EphyToolbarEditorPrivate));
}

GType
ephy_toolbar_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyToolbarEditorClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_toolbar_editor_class_init,
			NULL,
			NULL,
			sizeof (EphyToolbarEditor),
			0,
			(GInstanceInitFunc) ephy_toolbar_editor_init
		};

		type = g_type_register_static (GTK_TYPE_DIALOG,
					       "EphyToolbarEditor",
					       &our_info, 0);
	}

	return type;
}

GtkWidget *
ephy_toolbar_editor_show (EphyWindow *window)
{
	GtkWidget *dialog;

	dialog = GTK_WIDGET (g_object_get_data (G_OBJECT (window), DATA_KEY));
	if (dialog == NULL)
	{
		dialog = g_object_new (EPHY_TYPE_TOOLBAR_EDITOR,
				       "window", window,
				       NULL);

		g_object_set_data (G_OBJECT (window), DATA_KEY, dialog);
	}
		
	gtk_window_present (GTK_WINDOW (dialog));

	return dialog;
}
