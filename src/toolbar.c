/*
 *  Copyright (C) 2000 Marco Pesenti Gritti
 *            (C) 2001, 2002 Jorn Baayen
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

//#define DEBUG_MSG(x) g_print x
#define DEBUG_MSG(x)
#define NOT_IMPLEMENTED g_warning ("not implemented: " G_STRLOC);

#include "toolbar.h"
#include "ephy-spinner.h"
#include "ephy-window.h"
#include "ephy-bonobo-extensions.h"
#include "ephy-string.h"
#include "ephy-gui.h"
#include "ephy-location-entry.h"
#include "ephy-shell.h"
#include "ephy-embed-favicon.h"
#include "ephy-dnd.h"
#include "ephy-toolbar-bonobo-view.h"
#include "ephy-toolbar-item-factory.h"
#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"
#include "ephy-navigation-button.h"

#include <string.h>
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-window.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-ui-toolbar-button-item.h>
#include <bonobo/bonobo-property-bag.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkmenu.h>

#define DEFAULT_TOOLBAR_SETUP \
	"back_menu=navigation_button(direction=back,arrow=TRUE);" \
	"forward_menu=navigation_button(direction=forward,arrow=TRUE);" \
	"stop=std_toolitem(item=stop);" \
	"reload=std_toolitem(item=reload);" \
	"home=std_toolitem(item=home);" \
	"favicon=favicon;" \
	"location=location;" \
	"spinner=spinner;"

#define ZOOM_DELAY 50

static void toolbar_class_init (ToolbarClass *klass);
static void toolbar_init (Toolbar *t);
static void toolbar_finalize (GObject *object);
static void toolbar_set_window (Toolbar *t, EphyWindow *window);
static void toolbar_get_widgets (Toolbar *t);
static void toolbar_changed_cb (EphyToolbar *gt, Toolbar *t);
static void
toolbar_set_property (GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec);
static void
toolbar_get_property (GObject *object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec);


enum
{
	PROP_0,
	PROP_EPHY_WINDOW
};

enum
{
        TOOLBAR_ITEM_STYLE_PROP,
        TOOLBAR_ITEM_ORIENTATION_PROP,
	TOOLBAR_ITEM_PRIORITY_PROP
};

static GObjectClass *parent_class = NULL;

struct ToolbarPrivate
{
	EphyWindow *window;
	BonoboUIComponent *ui_component;
	EphyTbBonoboView *bview;

	GtkWidget *spinner;
	gboolean visibility;
	GtkWidget *location_entry;
	GSList *navigation_buttons;
	GtkTooltips *tooltips;
	GtkWidget *favicon;
	GtkWidget *favicon_ebox;
	GtkWidget *zoom_spinbutton;
	guint zoom_timeout_id;
	gboolean zoom_lock;
};

GType
toolbar_get_type (void)
{
        static GType toolbar_type = 0;

        if (toolbar_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (ToolbarClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) toolbar_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (Toolbar),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) toolbar_init
                };

                toolbar_type = g_type_register_static (EPHY_TYPE_TOOLBAR,
						       "Toolbar",
						       &our_info, 0);
        }

        return toolbar_type;

}

static void
toolbar_class_init (ToolbarClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = toolbar_finalize;

	object_class->set_property = toolbar_set_property;
	object_class->get_property = toolbar_get_property;

	g_object_class_install_property (object_class,
                                         PROP_EPHY_WINDOW,
                                         g_param_spec_object ("EphyWindow",
                                                              "EphyWindow",
                                                              "Parent window",
                                                              EPHY_WINDOW_TYPE,
                                                              G_PARAM_READWRITE));
	ephy_toolbar_item_register_type
		("navigation_button", (EphyTbItemConstructor) ephy_navigation_button_new);
}

static void
toolbar_set_property (GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
        Toolbar *t = TOOLBAR (object);

        switch (prop_id)
        {
		case PROP_EPHY_WINDOW:
		toolbar_set_window (t, g_value_get_object (value));
		break;
        }
}

static void
toolbar_get_property (GObject *object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec)
{
        Toolbar *t = TOOLBAR (object);

        switch (prop_id)
        {
                case PROP_EPHY_WINDOW:
                        g_value_set_object (value, t->priv->window);
                        break;
        }
}

static void
toolbar_location_url_activate_cb (EphyLocationEntry *entry,
				  const char *content,
				  const char *target,
				  EphyWindow *window)
{
	EphyBookmarks *bookmarks;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);

	if (!content)
	{
		ephy_window_load_url (window, target);
	}
	else
	{
		char *url;

		url = ephy_bookmarks_solve_smart_url
			(bookmarks, target, content);
		g_return_if_fail (url != NULL);
		ephy_window_load_url (window, url);
		g_free (url);
	}
}

static void
each_url_get_data_binder (EphyDragEachSelectedItemDataGet iteratee,
			  gpointer iterator_context, gpointer data)
{
	const char *location;
	EphyTab *tab;
	EphyWindow *window = EPHY_WINDOW(iterator_context);

	tab = ephy_window_get_active_tab (window);
	location = ephy_tab_get_location (tab);

	iteratee (location, -1, -1, -1, -1, data);
}

static void
favicon_drag_data_get_cb (GtkWidget *widget,
                          GdkDragContext *context,
                          GtkSelectionData *selection_data,
                          guint info,
                          guint32 time,
                          EphyWindow *window)
{
        g_assert (widget != NULL);
        g_return_if_fail (context != NULL);

        ephy_dnd_drag_data_get (widget, context, selection_data,
                info, time, window, each_url_get_data_binder);
}

static void
toolbar_setup_favicon_ebox (Toolbar *t, GtkWidget *w)
{
	ToolbarPrivate *p = t->priv;

	g_return_if_fail (w == p->favicon_ebox);

	p->favicon = g_object_ref (ephy_embed_favicon_new
				   (ephy_window_get_active_embed (p->window)));
	gtk_container_add (GTK_CONTAINER (p->favicon_ebox), p->favicon);
	gtk_container_set_border_width (GTK_CONTAINER (p->favicon_ebox), 2);

	ephy_dnd_url_drag_source_set (p->favicon_ebox);

	g_signal_connect (G_OBJECT (p->favicon_ebox),
			  "drag_data_get",
			  G_CALLBACK (favicon_drag_data_get_cb),
			  p->window);
	gtk_widget_show_all (p->favicon_ebox);
}

static gboolean
toolbar_zoom_timeout_cb (gpointer data)
{
	Toolbar *t = data;
	gint zoom = toolbar_get_zoom (t);

	g_return_val_if_fail (IS_EPHY_WINDOW (t->priv->window), FALSE);

	ephy_window_set_zoom (t->priv->window, zoom);

	return FALSE;
}

static void
toolbar_zoom_spinbutton_value_changed_cb (GtkSpinButton *sb, Toolbar *t)
{
	ToolbarPrivate *p = t->priv;
	if (p->zoom_timeout_id != 0)
	{
		g_source_remove (p->zoom_timeout_id);
	}
	if (!p->zoom_lock)
	{
		p->zoom_timeout_id = g_timeout_add (ZOOM_DELAY, toolbar_zoom_timeout_cb, t);
	}
}

static void
toolbar_setup_zoom_spinbutton (Toolbar *t, GtkWidget *w)
{
	g_signal_connect (w, "value_changed",
			  G_CALLBACK (toolbar_zoom_spinbutton_value_changed_cb), t);
	gtk_tooltips_set_tip (t->priv->tooltips, w, _("Zoom"), NULL);
}

static void
toolbar_setup_location_entry (Toolbar *t, GtkWidget *w)
{
	EphyAutocompletion *ac = ephy_shell_get_autocompletion (ephy_shell);
	EphyLocationEntry *e;

	g_return_if_fail (w == t->priv->location_entry);
	g_return_if_fail (EPHY_IS_LOCATION_ENTRY (w));

	e = EPHY_LOCATION_ENTRY (w);
	ephy_location_entry_set_autocompletion (e, ac);

	g_signal_connect (e, "activated",
			  GTK_SIGNAL_FUNC(toolbar_location_url_activate_cb),
			  t->priv->window);
}

static void
toolbar_setup_spinner (Toolbar *t, GtkWidget *w)
{
	ToolbarPrivate *p = t->priv;
        GtkWidget *spinner;

	g_return_if_fail (w == p->spinner);

        /* build the spinner and insert it into the box */
        spinner = ephy_spinner_new ();
	ephy_spinner_set_small_mode (EPHY_SPINNER (spinner), TRUE);
	gtk_container_add (GTK_CONTAINER (p->spinner), spinner);
	gtk_widget_show (spinner);

	/* don't care about the box anymore */
	g_object_unref (p->spinner);
	p->spinner = g_object_ref (spinner);
}


static void
toolbar_set_window (Toolbar *t, EphyWindow *window)
{
	g_return_if_fail (t->priv->window == NULL);

	t->priv->window = window;
	t->priv->ui_component = g_object_ref (t->priv->window->ui_component);

	ephy_tb_bonobo_view_set_path (t->priv->bview, t->priv->ui_component, "/Toolbar");

	toolbar_get_widgets (t);
}

static void
toolbar_get_widgets (Toolbar *t)
{
	ToolbarPrivate *p;
	EphyToolbar *gt;
	EphyTbItem *it;
	GSList *li;
	const gchar *nav_buttons_ids[] = {"back", "back_menu", "up", "up_menu", "forward", "forward_menu" };
	guint i;

	DEBUG_MSG (("in toolbar_get_widgets\n"));

	g_return_if_fail (IS_TOOLBAR (t));
	p = t->priv;
	g_return_if_fail (IS_EPHY_WINDOW (p->window));
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (p->ui_component));

	/* release all the widgets */

	for (li = p->navigation_buttons; li; li = li->next)
	{
		g_object_unref (li->data);
	}
	g_slist_free (p->navigation_buttons);
	p->navigation_buttons = NULL;

	if (p->favicon_ebox)
	{
		g_object_unref (p->favicon_ebox);
		p->favicon_ebox = NULL;
	}

	if (p->favicon)
	{
		g_object_unref (p->favicon);
		p->favicon = NULL;
	}

	if (p->location_entry)
	{
		g_object_unref (p->location_entry);
		p->location_entry = NULL;
	}

	if (p->spinner)
	{
		g_object_unref (p->spinner);
		p->spinner = NULL;
	}

	if (p->zoom_spinbutton)
	{
		g_object_unref (p->zoom_spinbutton);
		p->zoom_spinbutton = NULL;
	}

	gt = EPHY_TOOLBAR (t);

	for (i = 0; i < G_N_ELEMENTS (nav_buttons_ids); ++i)
	{
		it = ephy_toolbar_get_item_by_id (gt, nav_buttons_ids[i]);
		if (it)
		{
			if (EPHY_IS_NAVIGATION_BUTTON (it))
			{
				DEBUG_MSG (("    got a navigation button\n"));
				p->navigation_buttons = g_slist_prepend (p->navigation_buttons, g_object_ref (it));
				if (p->window)
				{
					ephy_tbi_set_window (EPHY_TBI (it), p->window);
				}
			}
			else
			{
				g_warning ("An unexpected button has been found in your toolbar. "
					   "Maybe your setup is too old.");
			}
		}
	}

	it = ephy_toolbar_get_item_by_id (gt, "location");
	if (it)
	{
		p->location_entry = ephy_tb_item_get_widget (it);
		g_object_ref (p->location_entry);
		toolbar_setup_location_entry (t, p->location_entry);

		DEBUG_MSG (("    got a location entry\n"));
	}

	it = ephy_toolbar_get_item_by_id (gt, "favicon");
	if (it)
	{
		p->favicon_ebox = ephy_tb_item_get_widget (it);
		g_object_ref (p->favicon_ebox);
		toolbar_setup_favicon_ebox (t, p->favicon_ebox);

		DEBUG_MSG (("    got a favicon ebox\n"));
	}

	it = ephy_toolbar_get_item_by_id (gt, "spinner");
	if (it)
	{
		p->spinner = ephy_tb_item_get_widget (it);
		g_object_ref (p->spinner);
		toolbar_setup_spinner (t, p->spinner);

		DEBUG_MSG (("    got a spinner\n"));
	}

	it = ephy_toolbar_get_item_by_id (gt, "zoom");
	if (it)
	{
		p->zoom_spinbutton = ephy_tb_item_get_widget (it);
		g_object_ref (p->zoom_spinbutton);
		toolbar_setup_zoom_spinbutton (t, p->zoom_spinbutton);

		DEBUG_MSG (("    got a zoom control\n"));
	}

	/* update the controls */
	ephy_window_update_all_controls (p->window);
}

static void
toolbar_init (Toolbar *t)
{
        t->priv = g_new0 (ToolbarPrivate, 1);

	t->priv->window = NULL;
	t->priv->ui_component = NULL;
	t->priv->navigation_buttons = NULL;
	t->priv->visibility = TRUE;
	t->priv->tooltips = gtk_tooltips_new ();
	g_object_ref (t->priv->tooltips);
	gtk_object_sink (GTK_OBJECT (t->priv->tooltips));

	if (!ephy_toolbar_listen_to_gconf (EPHY_TOOLBAR (t), CONF_TOOLBAR_SETUP))
	{
		/* FIXME: make this a dialog? */
		g_warning ("An incorrect toolbar configuration has been found, resetting to the default");

		/* this is to make sure we get a toolbar, even if the
		   setup is wrong or there is no schema */
		eel_gconf_set_string (CONF_TOOLBAR_SETUP, DEFAULT_TOOLBAR_SETUP);
	}

	g_signal_connect (t, "changed", G_CALLBACK (toolbar_changed_cb), t);

	t->priv->bview = ephy_tb_bonobo_view_new ();
	ephy_tb_bonobo_view_set_toolbar (t->priv->bview, EPHY_TOOLBAR (t));
}

static void
toolbar_changed_cb (EphyToolbar *gt, Toolbar *t)
{
	g_return_if_fail (gt == EPHY_TOOLBAR (t));

	if (t->priv->window)
	{
		toolbar_get_widgets (t);
	}
}

static void
toolbar_finalize (GObject *object)
{
	Toolbar *t;
	ToolbarPrivate *p;
	GSList *li;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_TOOLBAR (object));

	t = TOOLBAR (object);
	p = t->priv;

        g_return_if_fail (p != NULL);

	if (p->location_entry) g_object_unref (p->location_entry);
	if (p->favicon_ebox) g_object_unref (p->favicon_ebox);
	if (p->favicon) g_object_unref (p->favicon);
	if (p->spinner) g_object_unref (p->spinner);
	if (p->tooltips) g_object_unref (p->tooltips);
	if (p->zoom_spinbutton) g_object_unref (p->zoom_spinbutton);
	if (p->zoom_timeout_id != 0)
	{
		g_source_remove (p->zoom_timeout_id);
	}

	for (li = t->priv->navigation_buttons; li; li = li->next)
	{
		g_object_unref (li->data);
	}
	g_slist_free (t->priv->navigation_buttons);

	g_object_unref (t->priv->bview);

        g_free (t->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

Toolbar *
toolbar_new (EphyWindow *window)
{
	Toolbar *t;

	t = TOOLBAR (g_object_new (TOOLBAR_TYPE,
				   "EphyWindow", window,
				   NULL));

	g_return_val_if_fail (t->priv != NULL, NULL);

	return t;
}

void
toolbar_set_visibility (Toolbar *t, gboolean visibility)
{
	if (visibility == t->priv->visibility) return;

	t->priv->visibility = visibility;

	ephy_bonobo_set_hidden (BONOBO_UI_COMPONENT(t->priv->ui_component),
                               "/Toolbar",
                               !visibility);
}

void
toolbar_activate_location (Toolbar *t)
{
	if (t->priv->location_entry)
	{
		ephy_location_entry_activate
			(EPHY_LOCATION_ENTRY(t->priv->location_entry));
	}
}

void
toolbar_spinner_start (Toolbar *t)
{
	if (t->priv->spinner)
	{
		ephy_spinner_start (EPHY_SPINNER(t->priv->spinner));
	}
}

void
toolbar_spinner_stop (Toolbar *t)
{
	if (t->priv->spinner)
	{
		ephy_spinner_stop (EPHY_SPINNER(t->priv->spinner));
	}
}

static void
toolbar_navigation_button_set_sensitive (Toolbar *t, EphyNavigationDirection d, gboolean sensitivity)
{
	GSList *li;
	ToolbarPrivate *p = t->priv;

	for (li = p->navigation_buttons; li; li = li->next)
	{
		EphyNavigationButton *b = EPHY_NAVIGATION_BUTTON (li->data);
		if (ephy_navigation_button_get_direction (b) == d)
		{
			ephy_navigation_button_set_sensitive (b, sensitivity);
		}
	}
}

void
toolbar_button_set_sensitive (Toolbar *t,
			      ToolbarButtonID id,
			      gboolean sensitivity)
{
	switch (id)
	{
	case TOOLBAR_BACK_BUTTON:
		toolbar_navigation_button_set_sensitive (t, EPHY_NAVIGATION_DIRECTION_BACK, sensitivity);
		break;
	case TOOLBAR_FORWARD_BUTTON:
		toolbar_navigation_button_set_sensitive (t, EPHY_NAVIGATION_DIRECTION_FORWARD, sensitivity);
		break;
	case TOOLBAR_UP_BUTTON:
		toolbar_navigation_button_set_sensitive (t, EPHY_NAVIGATION_DIRECTION_UP, sensitivity);
		break;
	case TOOLBAR_STOP_BUTTON:
		ephy_bonobo_set_sensitive (t->priv->ui_component,
					  "/commands/GoStop",
					  sensitivity);
		break;
	}
}

void
toolbar_set_location (Toolbar *t,
		      const char *location)
{
	g_return_if_fail (location != NULL);

	if (t->priv->location_entry)
	{
		ephy_location_entry_set_location
			(EPHY_LOCATION_ENTRY (t->priv->location_entry), location);
	}
}

void
toolbar_update_favicon (Toolbar *t)
{
	if (t->priv->favicon)
	{
		ephy_embed_favicon_set_embed (EPHY_EMBED_FAVICON (t->priv->favicon),
				              ephy_window_get_active_embed (t->priv->window));
	}
}

char *
toolbar_get_location (Toolbar *t)
{
	gchar *location;
	if (t->priv->location_entry)
	{
		location = ephy_location_entry_get_location
			(EPHY_LOCATION_ENTRY (t->priv->location_entry));
	}
	else
	{
		location = g_strdup ("");
	}
	return location;
}

gint
toolbar_get_zoom (Toolbar *t)
{
	gint zoom;
	if (t->priv->zoom_spinbutton)
	{
		zoom = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (t->priv->zoom_spinbutton));
	}
	else
	{
		zoom = 100;
	}
	return zoom;
}

void
toolbar_set_zoom (Toolbar *t, gint zoom)
{
	ToolbarPrivate *p = t->priv;
	if (p->zoom_spinbutton)
	{
		p->zoom_lock = TRUE;
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (p->zoom_spinbutton), zoom);
		p->zoom_lock = FALSE;
	}
}

