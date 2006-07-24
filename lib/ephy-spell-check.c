/*
 *  Copyright (C) 2006 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>

#include <enchant.h>

#include "ephy-debug.h"

#include "ephy-spell-check.h"

#define EPHY_SPELL_CHECK_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_SPELL_CHECK, EphySpellCheckPrivate))

struct _EphySpellCheckPrivate
{
	EnchantBroker *broker;
	EnchantDict *dict;
};

enum
{
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static GObjectClass *parent_class;

/* Helper functions */

/* Class implementation */

static void
ephy_spell_check_init (EphySpellCheck *speller)
{
	EphySpellCheckPrivate *priv;
	const gchar * const *locale;

	priv = speller->priv = EPHY_SPELL_CHECK_GET_PRIVATE (speller);

	priv->broker = enchant_broker_init ();

	/* We don't want to check against C so we get a useful message 
           in case of no available dictionary */
	for (locale = g_get_language_names (); 
	     *locale != NULL && g_ascii_strcasecmp (*locale, "C") != 0;
	     ++locale)
	{
		priv->dict = enchant_broker_request_dict (priv->broker, *locale);
		if (priv->dict != NULL) break;
	}
	if (priv->dict == NULL)
		g_warning (enchant_broker_get_error (priv->broker));
}

static void
ephy_spell_check_finalize (GObject *object)
{
	EphySpellCheck *speller = EPHY_SPELL_CHECK (object);
	EphySpellCheckPrivate *priv = speller->priv;


	LOG ("EphySpellCheck finalised");

	if (priv->dict != NULL)
	{
		enchant_broker_free_dict (priv->broker, priv->dict);
		priv->dict = NULL;
	}

	enchant_broker_free (priv->broker);

	parent_class->finalize (object);
}

static void
ephy_spell_check_class_init (EphySpellCheckClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_spell_check_finalize;

#if 0
	signals[CANCEL] =
		g_signal_new ("cancel",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EphySpellCheckClass, cancel),
			      g_signal_accumulator_true_handled, NULL,
			      ephy_marshal_BOOLEAN__VOID,
			      G_TYPE_BOOLEAN,
			      0);
#endif

	g_type_class_add_private (object_class, sizeof (EphySpellCheckPrivate));
}

GType
ephy_spell_check_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphySpellCheckClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_spell_check_class_init,
			NULL,
			NULL,
			sizeof (EphySpellCheck),
			0,
			(GInstanceInitFunc) ephy_spell_check_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EphySpellCheck",
					       &our_info, 0);
	}

	return type;
}

/* Public API */

/**
 * ephy_spell_check_get_default:
 * 
 * Returns: a reference to the default #EphySpellCheck object
 */
EphySpellCheck *
ephy_spell_check_get_default (void)
{
	static EphySpellCheck *instance = NULL;

	if (instance == NULL)
	{
		EphySpellCheck **instanceptr = &instance;

		instance = g_object_new (EPHY_TYPE_SPELL_CHECK, NULL);
		g_object_add_weak_pointer (G_OBJECT (instance),
					   (gpointer) instanceptr);

		return instance;
	}
	
	return g_object_ref (instance);
}

int
ephy_spell_check_check_word (EphySpellCheck *speller,
			     const char *word,
			     gssize len,
			     gboolean *correct)
{
	EphySpellCheckPrivate *priv = speller->priv;
	int result;

	g_return_val_if_fail (word != NULL, -1);

	g_print ("ephy_spell_check_check_word: '%s'\n", word);

	if (priv->dict == NULL)
		return FALSE;

	if (len < 0)
		len = strlen (word);

	result = enchant_dict_check (priv->dict, word, len);
	if (result < 0)
		return FALSE;

	*correct = result == 0;

	return TRUE;
}

char **
ephy_spell_check_get_suggestions (EphySpellCheck *speller,
				  const char *word,
				  gssize len,
				  gsize *_count)
{
	EphySpellCheckPrivate *priv = speller->priv;
	char **suggestions;
	size_t count;

	g_return_val_if_fail (word != NULL, NULL);

	if (priv->dict == NULL)
		return FALSE;

	if (len < 0)
		len = strlen (word);

	suggestions =  enchant_dict_suggest (priv->dict, word, len, &count);
	
	*_count = count;
	return suggestions;
}

void
ephy_spell_check_free_suggestions (EphySpellCheck *speller,
				   char **suggestions)
{
	EphySpellCheckPrivate *priv = speller->priv;

	if (suggestions != NULL)
	{
		g_return_if_fail (priv->dict != NULL);

		/* FIXME!! What if inbetween there has been a change of dict!? */
		enchant_dict_free_suggestions (priv->dict, suggestions);
	}
}

gboolean
ephy_spell_check_set_language (EphySpellCheck *speller,
			       const char *lang)
{
	EphySpellCheckPrivate *priv = speller->priv;
	char *code;

	if (priv->dict != NULL)
	{
		enchant_broker_free_dict (priv->broker, priv->dict);
		priv->dict = NULL;
	}

	/* Enchant expects ab_CD codes, not ab-CD */
	code = g_strdup (lang);
	g_strdelimit (code, "-", '_');
	priv->dict = enchant_broker_request_dict (priv->broker, code);
	g_free (code);

	return priv->dict != NULL;
}

static void
describe_cb (const char * const lang_tag,
             const char * const provider_name,
	     const char * const provider_desc,
	     const char * const provider_file,
	     char **_language)
{
	*_language = g_strdup (lang_tag);
}

char *
ephy_spell_check_get_language (EphySpellCheck *speller)
{
	EphySpellCheckPrivate *priv = speller->priv;
	char *code = NULL;

	if (priv->dict == NULL) return NULL;

	enchant_dict_describe (priv->dict, (EnchantDictDescribeFn) describe_cb, &code);
	return code;
}
