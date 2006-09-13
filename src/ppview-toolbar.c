/*
 *  Copyright © 2002 Marco Pesenti Gritti
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

#include "config.h"

#include "ppview-toolbar.h"
#include "ephy-window.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkuimanager.h>

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
	PROP_WINDOW
};

static GObjectClass *parent_class = NULL;

#define EPHY_PPVIEW_TOOLBAR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_PPVIEW_TOOLBAR, PPViewToolbarPrivate))

struct PPViewToolbarPrivate
{
	EphyWindow *window;
	GtkUIManager *manager;
	GtkActionGroup *action_group;
	guint ui_id;
	int current_page;
};

static void
toolbar_cmd_ppv_goto_first (GtkUIManager *merge,
			    PPViewToolbar *t);

static void
toolbar_cmd_ppv_goto_last (GtkUIManager *merge,
			   PPViewToolbar *t);

static void
toolbar_cmd_ppv_go_back (GtkUIManager *merge,
			 PPViewToolbar *t);

static void
toolbar_cmd_ppv_go_forward (GtkUIManager *merge,
			    PPViewToolbar *t);

static void
toolbar_cmd_ppv_close (GtkUIManager *merge,
		       PPViewToolbar *t);

static const GtkActionEntry entries [] = {
	{ "PPVGotoFirst", GTK_STOCK_GOTO_FIRST,
	  N_("First"), NULL,
	  N_("Go to the first page"),
	  (GCallback)toolbar_cmd_ppv_goto_first },
	{ "PPVGotoLast", GTK_STOCK_GOTO_LAST,
	  N_("Last"), NULL,
	  N_("Go to the last page"),
	  (GCallback)toolbar_cmd_ppv_goto_last },
	{ "PPVGoBack", GTK_STOCK_GO_BACK,
	  N_("Previous"), NULL,
	  N_("Go to the previous page"),
	  (GCallback)toolbar_cmd_ppv_go_back },
	{ "PPVGoForward", GTK_STOCK_GO_FORWARD,
	  N_("Next"), NULL,
	  N_("Go to next page"),
	  (GCallback)toolbar_cmd_ppv_go_forward },
	{ "PPVClose", GTK_STOCK_CLOSE,
	  N_("Close"), NULL,
	  N_("Close print preview"),
	  (GCallback)toolbar_cmd_ppv_close },
};

static const char ui_info[] =
"<ui>"
"<toolbar name=\"PPViewToolbar\">"
"<toolitem name=\"PPVGotoFirstItem\" action=\"PPVGotoFirst\" />"
"<toolitem name=\"PPVGotoLastItem\" action=\"PPVGotoLast\" />"
"<toolitem name=\"PPVGoBackItem\" action=\"PPVGoBack\" />"
"<toolitem name=\"PPVGoForwardItem\" action=\"PPVGoForward\" />"
"<toolitem name=\"PPVCloseItem\" action=\"PPVClose\" />"
"</toolbar>"
"</ui>\n";

GType
ppview_toolbar_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
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

		type = g_type_register_static (G_TYPE_OBJECT,
					       "PPViewToolbar",
					       &our_info, 0);
        }

	return type;
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
                                         PROP_WINDOW,
                                         g_param_spec_object ("window",
                                                              "Window",
                                                              "Window",
                                                              EPHY_TYPE_WINDOW,
							      G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (PPViewToolbarPrivate));
}

static void
ppview_toolbar_set_property (GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
        PPViewToolbar *t = EPHY_PPVIEW_TOOLBAR (object);

        switch (prop_id)
        {
                case PROP_WINDOW:
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
	/* no readable properties */
	g_return_if_reached ();
}

static void
toolbar_update_sensitivity (PPViewToolbar *t)
{
	int pages, c_page;
	EphyWindow *window = t->priv->window;
	EphyEmbed *embed;
	GtkAction *action;
	GtkActionGroup *action_group = t->priv->action_group;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	pages = ephy_embed_print_preview_n_pages (embed);
	c_page = t->priv->current_page;

	action = gtk_action_group_get_action (action_group, "PPVGoBack");
	gtk_action_set_sensitive (action, c_page > 1);
	action = gtk_action_group_get_action (action_group, "PPVGotoFirst");
	gtk_action_set_sensitive (action, c_page > 1);
	action = gtk_action_group_get_action (action_group, "PPVGoForward");
	gtk_action_set_sensitive (action, c_page < pages);
	action = gtk_action_group_get_action (action_group, "PPVGotoLast");
	gtk_action_set_sensitive (action, c_page < pages);
}

static void
ppview_toolbar_set_window (PPViewToolbar *t, EphyWindow *window)
{
	g_return_if_fail (t->priv->window == NULL);

	t->priv->window = window;
	t->priv->manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (t->priv->window));

	t->priv->action_group = gtk_action_group_new ("PPViewActions");
	gtk_action_group_set_translation_domain (t->priv->action_group, NULL);
	gtk_action_group_add_actions (t->priv->action_group, entries,	
				      G_N_ELEMENTS (entries), t);
	gtk_ui_manager_insert_action_group (t->priv->manager,
					    t->priv->action_group, 0);
	t->priv->ui_id = gtk_ui_manager_add_ui_from_string
		(t->priv->manager, ui_info, -1, NULL);

	toolbar_update_sensitivity (t);
}

static void
ppview_toolbar_init (PPViewToolbar *t)
{
	t->priv = EPHY_PPVIEW_TOOLBAR_GET_PRIVATE (t);

	t->priv->current_page = 1;
}

static void
ppview_toolbar_finalize (GObject *object)
{
	PPViewToolbar *t = EPHY_PPVIEW_TOOLBAR (object);

	gtk_ui_manager_remove_ui (t->priv->manager, t->priv->ui_id);
	gtk_ui_manager_remove_action_group (t->priv->manager,
					    t->priv->action_group);
	g_object_unref (t->priv->action_group);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

PPViewToolbar *
ppview_toolbar_new (EphyWindow *window)
{
	return g_object_new (EPHY_TYPE_PPVIEW_TOOLBAR,
			     "window", window,
			     NULL);
}

static void
toolbar_cmd_ppv_goto_first (GtkUIManager *merge,
			    PPViewToolbar *t)
{
	EphyWindow *window = t->priv->window;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (EPHY_IS_EMBED (embed));

	ephy_embed_print_preview_navigate (embed, EPHY_EMBED_PRINTPREVIEW_HOME, 0);

	t->priv->current_page = 1;

	toolbar_update_sensitivity (t);
}

static void
toolbar_cmd_ppv_goto_last  (GtkUIManager *merge,
			    PPViewToolbar *t)
{
	EphyWindow *window = t->priv->window;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_print_preview_navigate (embed,
					   EPHY_EMBED_PRINTPREVIEW_END,
					   0);

	t->priv->current_page = ephy_embed_print_preview_n_pages (embed);

	toolbar_update_sensitivity (t);
}

static int
clamp_page_limits (PPViewToolbar *t, int page)
{
	EphyWindow *window = t->priv->window;
	EphyEmbed *embed;
	int pages;

	embed = ephy_window_get_active_embed (window);
	g_return_val_if_fail (embed != NULL, -1);

	pages = ephy_embed_print_preview_n_pages (embed);

	return CLAMP (page, 1, pages);
}

static void
toolbar_cmd_ppv_go_back  (GtkUIManager *merge,
			  PPViewToolbar *t)
{
	EphyWindow *window = t->priv->window;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (EPHY_IS_EMBED (embed));

	t->priv->current_page = clamp_page_limits (t, t->priv->current_page - 1);

	ephy_embed_print_preview_navigate (embed,
					   EPHY_EMBED_PRINTPREVIEW_GOTO_PAGENUM,
					   t->priv->current_page);

	toolbar_update_sensitivity (t);
}

static void
toolbar_cmd_ppv_go_forward (GtkUIManager *merge,
			    PPViewToolbar *t)
{
	EphyWindow *window = t->priv->window;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (EPHY_IS_EMBED (embed));

	t->priv->current_page = clamp_page_limits (t, t->priv->current_page + 1);

	ephy_embed_print_preview_navigate (embed,
					   EPHY_EMBED_PRINTPREVIEW_GOTO_PAGENUM,
					   t->priv->current_page);

	toolbar_update_sensitivity (t);
}

static void
toolbar_cmd_ppv_close (GtkUIManager *merge,
		       PPViewToolbar *t)
{
	EphyWindow *window;
	EphyEmbed *embed;

	g_return_if_fail (EPHY_IS_PPVIEW_TOOLBAR (t));

	window = t->priv->window;
	g_return_if_fail (EPHY_IS_WINDOW (window));

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (EPHY_IS_EMBED (embed));

	ephy_window_set_print_preview (window, FALSE);

	ephy_embed_set_print_preview_mode (embed, FALSE);
}
