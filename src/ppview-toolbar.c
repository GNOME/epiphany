/*
 *  Copyright (C) 2002 Marco Pesenti Gritti
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


#include "ppview-toolbar.h"
#include "ephy-window.h"
#include "ephy-bonobo-extensions.h"
#include "ephy-string.h"
#include "ephy-gui.h"
#include "egg-menu-merge.h"

#include <string.h>
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-window.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-ui-toolbar-button-item.h>
#include <bonobo/bonobo-property-bag.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkmenu.h>

#define PPV_GOTO_FIRST_PATH "/commands/PPVGotoFirst"
#define PPV_GOTO_LAST_PATH "/commands/PPVGotoLast"
#define PPV_GO_BACK_PATH "/commands/PPVGoBack"
#define PPV_GO_FORWARD_PATH "/commands/PPVGoForward"

static void ppview_toolbar_class_init (PPViewToolbarClass *klass);
static void ppview_toolbar_init (PPViewToolbar *t);
static void ppview_toolbar_finalize (GObject *object);
static void ppview_toolbar_set_window (PPViewToolbar *t, EphyWindow *window);
static void
ppview_toolbar_set_property (GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec);
static void
ppview_toolbar_get_property (GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec);

enum
{
	PROP_0,
	PROP_EPHY_WINDOW
};

static GObjectClass *parent_class = NULL;

struct PPViewToolbarPrivate
{
	EphyWindow *window;
	EggMenuMerge *ui_merge;
	EggActionGroup *action_group;
	guint ui_id;
	int current_page;
};

static void
toolbar_cmd_ppv_goto_first (EggMenuMerge *merge,
			    PPViewToolbar *t);

static void
toolbar_cmd_ppv_goto_last (EggMenuMerge *merge,
			   PPViewToolbar *t);

static void
toolbar_cmd_ppv_go_back (EggMenuMerge *merge,
			 PPViewToolbar *t);

static void
toolbar_cmd_ppv_go_forward (EggMenuMerge *merge,
			    PPViewToolbar *t);

static void
toolbar_cmd_ppv_close (EggMenuMerge *merge,
		       PPViewToolbar *t);

static EggActionGroupEntry entries [] = {
	{ "PPVGotoFirst", N_("First"),
	  GTK_STOCK_GOTO_FIRST, NULL,
	  N_("Go to the first page"),
	  (GCallback)toolbar_cmd_ppv_goto_first, NULL },
	{ "PPVGotoLast", N_("Last"),
	  GTK_STOCK_GOTO_LAST, NULL,
	  N_("Go to the last page"),
	  (GCallback)toolbar_cmd_ppv_goto_last, NULL },
	{ "PPVGoBack", N_("Previous"),
	  GTK_STOCK_GO_BACK, NULL,
	  N_("Go to the previous page"),
	  (GCallback)toolbar_cmd_ppv_go_back, NULL },
	{ "PPVGoForward", N_("Next"),
	  GTK_STOCK_GO_FORWARD, NULL,
	  N_("Go to next page"),
	  (GCallback)toolbar_cmd_ppv_go_forward, NULL },
	{ "PPVClose", N_("Close"),
	  GTK_STOCK_CLOSE, NULL,
	  N_("Close print preview"),
	  (GCallback)toolbar_cmd_ppv_close, NULL },
};
static guint n_entries = G_N_ELEMENTS (entries);

static const gchar *ui_info =
"<Root>\n"
"  <dockitem name=\"PPViewToolbar\">\n"
"    <toolitem name=\"PPVGotoFirstItem\" verb=\"PPVGotoFirst\" />\n"
"    <toolitem name=\"PPVGotoLastItem\" verb=\"PPVGotoLast\" />\n"
"    <toolitem name=\"PPVGoBackItem\"verb=\"PPVGoBack\" />\n"
"    <toolitem name=\"PPVGoForwardItem\" verb=\"PPVGoForward\" />\n"
"    <toolitem name=\"PPVClose\" verb=\"PPVClose\" />\n"
"  </dockitem>\n"
"</Root>\n";

GType
ppview_toolbar_get_type (void)
{
        static GType ppview_toolbar_type = 0;

        if (ppview_toolbar_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (PPViewToolbarClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ppview_toolbar_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (PPViewToolbar),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) ppview_toolbar_init
                };

                ppview_toolbar_type = g_type_register_static (G_TYPE_OBJECT,
						       "PPViewToolbar",
						       &our_info, 0);
        }

        return ppview_toolbar_type;

}

static void
ppview_toolbar_class_init (PPViewToolbarClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = ppview_toolbar_finalize;

	object_class->set_property = ppview_toolbar_set_property;
	object_class->get_property = ppview_toolbar_get_property;

	g_object_class_install_property (object_class,
                                         PROP_EPHY_WINDOW,
                                         g_param_spec_object ("EphyWindow",
                                                              "EphyWindow",
                                                              "Parent window",
                                                              EPHY_WINDOW_TYPE,
                                                              G_PARAM_READWRITE));
}

static void
ppview_toolbar_set_property (GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
        PPViewToolbar *t = PPVIEW_TOOLBAR (object);

        switch (prop_id)
        {
                case PROP_EPHY_WINDOW:
                        ppview_toolbar_set_window (t, g_value_get_object (value));
                        break;
        }
}

static void
ppview_toolbar_get_property (GObject *object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec)
{
        PPViewToolbar *t = PPVIEW_TOOLBAR (object);

        switch (prop_id)
        {
                case PROP_EPHY_WINDOW:
                        g_value_set_object (value, t->priv->window);
                        break;
        }
}

static void
ppview_toolbar_set_window (PPViewToolbar *t, EphyWindow *window)
{
	g_return_if_fail (t->priv->window == NULL);

	t->priv->window = window;
	t->priv->ui_merge = EGG_MENU_MERGE (t->priv->window->ui_merge);

	t->priv->action_group = egg_action_group_new ("PPViewActions");
	egg_action_group_add_actions (t->priv->action_group, entries, n_entries);
	egg_menu_merge_insert_action_group (t->priv->ui_merge,
					    t->priv->action_group, 0);
	t->priv->ui_id = egg_menu_merge_add_ui_from_string
		(t->priv->ui_merge, ui_info, -1, NULL);
}

static void
ppview_toolbar_init (PPViewToolbar *t)
{
	int i;

        t->priv = g_new0 (PPViewToolbarPrivate, 1);

	t->priv->window = NULL;
	t->priv->ui_merge = NULL;

	for (i = 0; i < n_entries; i++)
	{
		entries[i].user_data = t;
	}
}

static void
ppview_toolbar_finalize (GObject *object)
{
	PPViewToolbar *t;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_PPVIEW_TOOLBAR (object));

	t = PPVIEW_TOOLBAR (object);

        g_return_if_fail (t->priv != NULL);
	egg_menu_merge_remove_ui (t->priv->ui_merge, t->priv->ui_id);
	g_object_unref (t->priv->action_group);

        g_free (t->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

PPViewToolbar *
ppview_toolbar_new (EphyWindow *window)
{
	PPViewToolbar *t;

	t = PPVIEW_TOOLBAR (g_object_new (PPVIEW_TOOLBAR_TYPE,
					  "EphyWindow", window,
					  NULL));

	g_return_val_if_fail (t->priv != NULL, NULL);

	return t;
}

static void
toolbar_update_sensitivity (PPViewToolbar *t)
{
	int pages, c_page;
	EphyWindow *window = t->priv->window;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_print_preview_num_pages (embed, &pages);
	c_page = t->priv->current_page;
/*
	ephy_bonobo_set_sensitive (t->priv->ui_component,
				  PPV_GO_BACK_PATH, c_page > 1);
	ephy_bonobo_set_sensitive (t->priv->ui_component,
				  PPV_GOTO_FIRST_PATH, c_page > 1);
	ephy_bonobo_set_sensitive (t->priv->ui_component,
				  PPV_GO_FORWARD_PATH, c_page < pages);
	ephy_bonobo_set_sensitive (t->priv->ui_component,
				  PPV_GOTO_LAST_PATH, c_page < pages);
				  */
}

static void
toolbar_cmd_ppv_goto_first (EggMenuMerge *merge,
			    PPViewToolbar *t)
{
	EphyWindow *window = t->priv->window;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_print_preview_navigate (embed, PRINTPREVIEW_HOME, 0);

	t->priv->current_page = 1;

	toolbar_update_sensitivity (t);
}

static void
toolbar_cmd_ppv_goto_last  (EggMenuMerge *merge,
			    PPViewToolbar *t)
{
	EphyWindow *window = t->priv->window;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_print_preview_navigate (embed,
					   PRINTPREVIEW_END,
					   0);

	ephy_embed_print_preview_num_pages (embed,
					    &t->priv->current_page);

	toolbar_update_sensitivity (t);
}

static void
toolbar_cmd_ppv_go_back  (EggMenuMerge *merge,
			  PPViewToolbar *t)
{
	EphyWindow *window = t->priv->window;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_print_preview_navigate (embed,
					   PRINTPREVIEW_PREV_PAGE,
					   0);

	t->priv->current_page --;

	toolbar_update_sensitivity (t);
}

static void
toolbar_cmd_ppv_go_forward (EggMenuMerge *merge,
			    PPViewToolbar *t)
{
	EphyWindow *window = t->priv->window;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_print_preview_navigate (embed,
					   PRINTPREVIEW_NEXT_PAGE,
					   0);

	t->priv->current_page ++;

	toolbar_update_sensitivity (t);
}

static void
toolbar_cmd_ppv_close (EggMenuMerge *merge,
		       PPViewToolbar *t)
{
}

