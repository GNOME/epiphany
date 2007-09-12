/*
 *  Copyright © 2006 Christian Persch
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#include <xpcom-config.h>
#include "config.h"

//#include <bonobo.h>
#include <stdio.h>
#include <stdlib.h>

#include <nsStringAPI.h>

#include <mozIPersonalDictionary.h>
#include <nsMemory.h>

#include "ephy-debug.h"

#include "GeckoSpellCheckEngine.h"

#ifndef HAVE_GECKO_1_9
#define ToNewUnicode NS_StringCloneData
#endif

GeckoSpellCheckEngine::GeckoSpellCheckEngine ()
{
  LOG ("GeckoSpellCheckEngine ctor [%p]", (void*) this);
  mSpeller = ephy_spell_check_get_default ();
}

GeckoSpellCheckEngine::~GeckoSpellCheckEngine ()
{
  LOG ("GeckoSpellCheckEngine dtor [%p]", (void*) this);
  g_object_unref (mSpeller);
}

NS_IMPL_ISUPPORTS1 (GeckoSpellCheckEngine,
		    mozISpellCheckingEngine)

/* nsISpellCheckEngine implementation */

/* attribute wstring dictionary; */
NS_IMETHODIMP GeckoSpellCheckEngine::GetDictionary (PRUnichar * *aDictionary)
{
  /* Gets the identifier of the current dictionary */
  char *code = ephy_spell_check_get_language (mSpeller);
  if (!code) {
    return NS_ERROR_FAILURE;
  }

  *aDictionary = ToNewUnicode (NS_ConvertUTF8toUTF16 (code));
  g_free (code);

  return NS_OK;
}

NS_IMETHODIMP GeckoSpellCheckEngine::SetDictionary (const PRUnichar * aDictionary)
{
  return NS_OK;
}

/* readonly attribute wstring language; */
NS_IMETHODIMP GeckoSpellCheckEngine::GetLanguage (PRUnichar * *aLanguage)
{
  /* Gets the identifier of the current dictionary */
  char *code = ephy_spell_check_get_language (mSpeller);
  if (!code) {
    return NS_ERROR_FAILURE;
  }

  *aLanguage = ToNewUnicode (NS_ConvertUTF8toUTF16 (code));
  g_free (code);

  return NS_OK;
}

/* readonly attribute boolean providesPersonalDictionary; */
NS_IMETHODIMP GeckoSpellCheckEngine::GetProvidesPersonalDictionary (PRBool *aProvidesPersonalDictionary)
{
  *aProvidesPersonalDictionary = PR_FALSE;
  return NS_OK;
}

/* readonly attribute boolean providesWordUtils; */
NS_IMETHODIMP GeckoSpellCheckEngine::GetProvidesWordUtils (PRBool *aProvidesWordUtils)
{
  *aProvidesWordUtils = PR_FALSE;
  return NS_OK;
}

/* readonly attribute wstring name; */
NS_IMETHODIMP GeckoSpellCheckEngine::GetName (PRUnichar * *aName)
{
  /* It's fine to leave this unimplemented */
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute wstring copyright; */
NS_IMETHODIMP GeckoSpellCheckEngine::GetCopyright (PRUnichar * *aCopyright)
{
  /* It's fine to leave this unimplemented */
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute mozIPersonalDictionary personalDictionary; */
NS_IMETHODIMP GeckoSpellCheckEngine::GetPersonalDictionary (mozIPersonalDictionary * *aPersonalDictionary)
{
  NS_IF_ADDREF (*aPersonalDictionary = mPersonalDictionary);
  return NS_OK;
}

NS_IMETHODIMP GeckoSpellCheckEngine::SetPersonalDictionary (mozIPersonalDictionary * aPersonalDictionary)
{
  mPersonalDictionary = aPersonalDictionary;
  return NS_OK;
}

/* void getDictionaryList ([array, size_is (count)] out wstring dictionaries, out PRUint32 count); */
NS_IMETHODIMP
GeckoSpellCheckEngine::GetDictionaryList (PRUnichar ***_dictionaries,
					  PRUint32 *_count)
{
  *_count = 1;
  *_dictionaries = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *)); // only one entry
  *_dictionaries[0] = ToNewUnicode (NS_LITERAL_STRING ("en"));
  return NS_OK;
}

/* boolean check (in wstring word); */
NS_IMETHODIMP GeckoSpellCheckEngine::Check (const PRUnichar *word,
					    PRBool *_retval)
{
  NS_ENSURE_STATE (mSpeller);
  NS_ENSURE_ARG (word);

  NS_ConvertUTF16toUTF8 converted (word);

  gboolean correct = FALSE;
  if (!ephy_spell_check_check_word (mSpeller,
       				    converted.get (),
				    converted.Length (),
				    &correct))
	  return NS_ERROR_FAILURE;

  *_retval = correct != FALSE;

  return NS_OK;
}

/* void suggest (in wstring word, [array, size_is (count)] out wstring suggestions, out PRUint32 count); */
NS_IMETHODIMP GeckoSpellCheckEngine::Suggest (const PRUnichar *word,
					      PRUnichar ***_suggestions,
					      PRUint32 *_count)
{
#if 0
  NS_ENSURE_STATE (mSpeller);
  NS_ENSURE_ARG (word);

  NS_ConvertUTF16toUTF8 converted (word);

  gsize count;
  char **suggestions = ephy_spell_check_get_suggestions (mSpeller,
		  					 converted.get (),
							 converted.Length (),
							 &count);

  *_count = count;
  *_suggestions = nsnull;

  PRUnichar **array = nsnull;
  if (count > 0) {
    NS_ASSERTION (suggestions, "Count > 0 but suggestions are NULL?");
    array = (PRUnichar **) nsMemory::Alloc (count * sizeof (PRUnichar *));
    if (array) {
      *_suggestions = array;

      for (gsize i = 0; i < count; ++i) {
        NS_ConvertUTF8toUTF16 sugg (suggestions[i]);
	array[i] = ToNewUnicode (sugg);
      }
    }

    ephy_spell_check_free_suggestions (mSpeller, suggestions);
  }

  return array ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
#endif
  return NS_ERROR_NOT_IMPLEMENTED;
}
