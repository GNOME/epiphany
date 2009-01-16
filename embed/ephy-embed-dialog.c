/*
 *  Copyright © 2000, 2001, 2002 Marco Pesenti Gritti
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

#include "ephy-embed-dialog.h"

static void
ephy_embed_dialog_class_init (EphyEmbedDialogClass *klass);
static void
ephy_embed_dialog_init (EphyEmbedDialog *window);
static void
ephy_embed_dialog_finalize (GObject *object);
static void
ephy_embed_dialog_get_property (GObject *object,
				guint prop_id,
				GValue *value,
				GParamSpec *pspec);
static void
ephy_embed_dialog_set_property (GObject *object,
				guint prop_id,
				const GValue *value,
				GParamSpec *pspec);

enum
{
	PROP_0,
	PROP_EPHY_EMBED
};

#define EPHY_EMBED_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED_DIALOG, EphyEmbedDialogPrivate))

struct _EphyEmbedDialogPrivate
{
	EphyEmbed *embed;
};

G_DEFINE_TYPE (EphyEmbedDialog, ephy_embed_dialog, EPHY_TYPE_DIALOG)

static void
ephy_embed_dialog_class_init (EphyEmbedDialogClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = ephy_embed_dialog_finalize;
	object_class->set_property = ephy_embed_dialog_set_property;
	object_class->get_property = ephy_embed_dialog_get_property;

	g_object_class_install_property (object_class,
					 PROP_EPHY_EMBED,
                                         g_param_spec_object ("embed",
                                                              "Embed",
                                                              "The dialog's embed",
                                                              G_TYPE_OBJECT,
                                                              G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_type_class_add_private (object_class, sizeof(EphyEmbedDialogPrivate));
}

static void
ephy_embed_dialog_init (EphyEmbedDialog *dialog)
{
        dialog->priv = EPHY_EMBED_DIALOG_GET_PRIVATE (dialog);
}

static void
unset_embed (EphyEmbedDialog *dialog)
{
	if (dialog->priv->embed != NULL)
	{
		EphyEmbed **embedptr;
		embedptr = &dialog->priv->embed;
		g_object_remove_weak_pointer (G_OBJECT (dialog->priv->embed),
					      (gpointer *) embedptr);
	}
}

static void
ephy_embed_dialog_finalize (GObject *object)
{
        EphyEmbedDialog *dialog = EPHY_EMBED_DIALOG (object);

	unset_embed (dialog);

        G_OBJECT_CLASS (ephy_embed_dialog_parent_class)->finalize (object);
}

static void
ephy_embed_dialog_set_property (GObject *object,
				guint prop_id,
				const GValue *value,
				GParamSpec *pspec)
{
        EphyEmbedDialog *d = EPHY_EMBED_DIALOG (object);

        switch (prop_id)
        {
                case PROP_EPHY_EMBED:
                        ephy_embed_dialog_set_embed (d, g_value_get_object (value));
                        break;
        }
}

static void
ephy_embed_dialog_get_property (GObject *object,
				guint prop_id,
				GValue *value,
				GParamSpec *pspec)
{
        EphyEmbedDialog *d = EPHY_EMBED_DIALOG (object);

        switch (prop_id)
        {
                case PROP_EPHY_EMBED:
                        g_value_set_object (value, d->priv->embed);
                        break;
        }
}

EphyEmbedDialog *
ephy_embed_dialog_new (EphyEmbed *embed)
{
	return EPHY_EMBED_DIALOG (g_object_new (EPHY_TYPE_EMBED_DIALOG,
						"embed", embed,
						NULL));
}

EphyEmbedDialog *
ephy_embed_dialog_new_with_parent (GtkWidget *parent_window,
				     EphyEmbed *embed)
{
	return EPHY_EMBED_DIALOG (g_object_new
				    (EPHY_TYPE_EMBED_DIALOG,
				     "parent-window", parent_window,
				     "embed", embed,
				     NULL));
}

void
ephy_embed_dialog_set_embed (EphyEmbedDialog *dialog,
			     EphyEmbed *embed)
{
	EphyEmbed **embedptr;

	unset_embed (dialog);
	dialog->priv->embed = embed;

	embedptr = &dialog->priv->embed;
	g_object_add_weak_pointer (G_OBJECT (dialog->priv->embed),
				   (gpointer *) embedptr);
	g_object_notify (G_OBJECT (dialog), "embed");
}

EphyEmbed *
ephy_embed_dialog_get_embed (EphyEmbedDialog *dialog)
{
	return dialog->priv->embed;
}
