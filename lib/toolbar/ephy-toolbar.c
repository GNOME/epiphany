/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libgnome/gnome-i18n.h>
#include <string.h>
#include "ephy-gobject-misc.h"
#include "ephy-marshal.h"
#include "ephy-toolbar.h"
#include "ephy-toolbar-item-factory.h"
#include "eel-gconf-extensions.h"

#define NOT_IMPLEMENTED g_warning ("not implemented: " G_STRLOC);
//#define DEBUG_MSG(x) g_print x
#define DEBUG_MSG(x)

/**
 * Private data
 */
struct _EphyToolbarPrivate
{
	GSList *items;
	guint gconf_notification_id;

	gboolean check_unique;
	gboolean fixed_order;
	GSList *order; /* list of ids */
};

/**
 * Private functions, only availble from this file
 */
static void		ephy_toolbar_class_init			(EphyToolbarClass *klass);
static void		ephy_toolbar_init			(EphyToolbar *tb);
static void		ephy_toolbar_finalize_impl		(GObject *o);
static void		ephy_toolbar_listen_to_gconf_cb		(GConfClient* client,
								 guint cnxn_id,
								 GConfEntry *entry,
								 gpointer user_data);
static void		ephy_toolbar_update_order		(EphyToolbar *tb);


static gpointer g_object_class;

/* signals enums and ids */
enum EphyToolbarSignalsEnum {
	EPHY_TOOLBAR_CHANGED,
	EPHY_TOOLBAR_LAST_SIGNAL
};
static gint EphyToolbarSignals[EPHY_TOOLBAR_LAST_SIGNAL];

/**
 * Toolbar object
 */

MAKE_GET_TYPE (ephy_toolbar, "EphyToolbar", EphyToolbar, ephy_toolbar_class_init,
	       ephy_toolbar_init, G_TYPE_OBJECT);

static void
ephy_toolbar_class_init (EphyToolbarClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = ephy_toolbar_finalize_impl;

	EphyToolbarSignals[EPHY_TOOLBAR_CHANGED] = g_signal_new (
		"changed", G_OBJECT_CLASS_TYPE (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP,
                G_STRUCT_OFFSET (EphyToolbarClass, changed),
		NULL, NULL,
		ephy_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	g_object_class = g_type_class_peek_parent (klass);
}

static void
ephy_toolbar_init (EphyToolbar *tb)
{
	EphyToolbarPrivate *p = g_new0 (EphyToolbarPrivate, 1);
	tb->priv = p;

	p->check_unique = TRUE;
}

static void
ephy_toolbar_finalize_impl (GObject *o)
{
	EphyToolbar *tb = EPHY_TOOLBAR (o);
	EphyToolbarPrivate *p = tb->priv;

	g_slist_foreach (p->items, (GFunc) g_object_unref, NULL);
	g_slist_free (p->items);

	if (p->gconf_notification_id)
	{
		eel_gconf_notification_remove (p->gconf_notification_id);
	}

	g_slist_foreach (p->order, (GFunc) g_free, NULL);
	g_slist_free (p->order);

	g_free (p);

	DEBUG_MSG (("EphyToolbar finalized\n"));

	G_OBJECT_CLASS (g_object_class)->finalize (o);
}


EphyToolbar *
ephy_toolbar_new (void)
{
	EphyToolbar *ret = g_object_new (EPHY_TYPE_TOOLBAR, NULL);
	return ret;
}

gboolean
ephy_toolbar_parse (EphyToolbar *tb, const gchar *cfg)
{
	EphyToolbarPrivate *p = tb->priv;
	GSList *list = NULL;
	gchar **items;
	int i;

	g_return_val_if_fail (EPHY_IS_TOOLBAR (tb), FALSE);
	g_return_val_if_fail (cfg != NULL, FALSE);

	items = g_strsplit (cfg, ";", 9999);
	if (!items) return FALSE;

	for (i = 0; items[i]; ++i)
	{
		if (items[i][0])
		{
			EphyTbItem *it = ephy_toolbar_item_create_from_string (items[i]);

			if (!it)
			{
				/* FIXME: this leaks everything... */
				return FALSE;
			}

			list = g_slist_prepend (list, it);
		}
	}

	g_strfreev (items);

	g_slist_foreach (p->items, (GFunc) g_object_unref, NULL);
	g_slist_free (p->items);
	p->items = g_slist_reverse (list);

	if (p->fixed_order)
	{
		ephy_toolbar_update_order (tb);
	}

	g_signal_emit (tb, EphyToolbarSignals[EPHY_TOOLBAR_CHANGED], 0);

	return TRUE;
}

gchar *
ephy_toolbar_to_string (EphyToolbar *tb)
{
	EphyToolbarPrivate *p = tb->priv;
	gchar *ret;
	GString *str = g_string_new ("");
	GSList *li;

	for (li = p->items; li; li = li->next)
	{
		EphyTbItem *it = li->data;
		gchar *s = ephy_tb_item_to_string (it);
		g_string_append (str, s);
		if (li->next)
		{
			g_string_append (str, ";");
		}
		g_free (s);
	}

	ret = str->str;
	g_string_free (str, FALSE);
	return ret;
}

static void
ephy_toolbar_listen_to_gconf_cb (GConfClient* client,
				guint cnxn_id,
				GConfEntry *entry,
				gpointer user_data)
{
	EphyToolbar *tb = user_data;
	GConfValue *value;
	const char *str;

	g_return_if_fail (EPHY_IS_TOOLBAR (tb));

	value = gconf_entry_get_value (entry);
	str = gconf_value_get_string (value);

	DEBUG_MSG (("in ephy_toolbar_listen_to_gconf_cb\n"));

	ephy_toolbar_parse (tb, str);
}

/**
 * Listen to changes in the toolbar configuration. Returns TRUE if the
 * current configuration is valid.
 */
gboolean
ephy_toolbar_listen_to_gconf (EphyToolbar *tb, const gchar *gconf_key)
{
	EphyToolbarPrivate *p = tb->priv;
	gchar *s;
	gboolean ret = FALSE;

	if (p->gconf_notification_id)
	{
		eel_gconf_notification_remove (p->gconf_notification_id);
	}

	s = eel_gconf_get_string (gconf_key);
	if (s)
	{
		ret = ephy_toolbar_parse (tb, s);
		g_free (s);
	}

	p->gconf_notification_id = eel_gconf_notification_add (gconf_key,
							       ephy_toolbar_listen_to_gconf_cb,
							       tb);

	DEBUG_MSG (("listening to %s, %d (FIXME: does not seem to work)\n",
		    gconf_key, p->gconf_notification_id));

	return ret;
}

EphyTbItem *
ephy_toolbar_get_item_by_id (EphyToolbar *tb, const gchar *id)
{
	EphyToolbarPrivate *p = tb->priv;
	GSList *li;

	for (li = p->items; li; li = li->next)
	{
		EphyTbItem *i = li->data;
		if (i->id && !strcmp (i->id, id))
		{
			return i;
		}
	}
	return NULL;
}

const GSList *
ephy_toolbar_get_item_list (EphyToolbar *tb)
{
	EphyToolbarPrivate *p = tb->priv;
	return p->items;
}

void
ephy_toolbar_add_item (EphyToolbar *tb, EphyTbItem *it, gint index)
{
	EphyToolbarPrivate *p = tb->priv;
	EphyTbItem *old_it;

	g_return_if_fail (g_slist_find (p->items, it) == NULL);

	if (p->check_unique && ephy_tb_item_is_unique (it)
	    && (old_it = ephy_toolbar_get_item_by_id (tb, it->id)) != NULL)
	{
		GSList *old_it_link;
		if (p->fixed_order)
		{
			return;
		}
		old_it_link = g_slist_find (p->items, old_it);
		p->items = g_slist_insert (p->items, old_it, index);
		p->items = g_slist_delete_link (p->items, old_it_link);

	}
	else
	{
		if (p->fixed_order)
		{
			GSList *li;
			if (ephy_toolbar_get_item_by_id (tb, it->id) != NULL)
			{
				return;
			}
			index = 0;
			for (li = p->order; li && strcmp (li->data, it->id); li = li->next)
			{
				if (ephy_toolbar_get_item_by_id (tb, li->data) != NULL)
				{
					++index;
				}
			}
		}

		p->items = g_slist_insert (p->items, it, index);
		g_object_ref (it);
	}
	g_signal_emit (tb, EphyToolbarSignals[EPHY_TOOLBAR_CHANGED], 0);
}

void
ephy_toolbar_remove_item (EphyToolbar *tb, EphyTbItem *it)
{
	EphyToolbarPrivate *p = tb->priv;

	g_return_if_fail (g_slist_find (p->items, it) != NULL);

	p->items = g_slist_remove (p->items, it);

	g_signal_emit (tb, EphyToolbarSignals[EPHY_TOOLBAR_CHANGED], 0);

	g_object_unref (it);
}

void
ephy_toolbar_set_fixed_order (EphyToolbar *tb, gboolean value)
{
	EphyToolbarPrivate *p = tb->priv;
	p->fixed_order = value;

	if (value)
	{
		ephy_toolbar_update_order (tb);
	}
}

void
ephy_toolbar_set_check_unique (EphyToolbar *tb, gboolean value)
{
	EphyToolbarPrivate *p = tb->priv;
	p->check_unique = value;

	/* maybe it should remove duplicated items now, if any */
}

gboolean
ephy_toolbar_get_check_unique (EphyToolbar *tb)
{
	EphyToolbarPrivate *p = tb->priv;
	return p->check_unique;
}

static void
ephy_toolbar_update_order (EphyToolbar *tb)
{
	EphyToolbarPrivate *p = tb->priv;
	GSList *li;
	GSList *lj;
	GSList *new_order = NULL;

	lj = p->order;
	for (li = p->items; li; li = li->next)
	{
		EphyTbItem *i = li->data;
		const gchar *id = i->id;

		if (g_slist_find_custom (lj, id, (GCompareFunc) strcmp))
		{
			for ( ; lj && strcmp (lj->data, id); lj = lj->next)
			{
				if (ephy_toolbar_get_item_by_id (tb, lj->data) == NULL)
				{
					new_order = g_slist_prepend (new_order, g_strdup (lj->data));
				}
			}
		}

		new_order = g_slist_prepend (new_order, g_strdup (id));

	}

	for ( ; lj; lj = lj->next)
	{
		if (ephy_toolbar_get_item_by_id (tb, lj->data) == NULL)
		{
			new_order = g_slist_prepend (new_order, g_strdup (lj->data));
		}
	}

	g_slist_foreach (p->order, (GFunc) g_free, NULL);
	g_slist_free (p->order);

	p->order = g_slist_reverse (new_order);

#ifdef DEBUG_ORDER
	DEBUG_MSG (("New order:\n"));
	for (lj = p->order; lj; lj = lj->next)
	{
		DEBUG_MSG (("%s\n", (char *) lj->data));
	}
#endif
}

