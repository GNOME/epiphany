/*
 *  Copyright (C) 2000-2002 Marco Pesenti Gritti
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

#include "mozilla-prefs.h"

#include <nsCOMPtr.h>
#include <nsIPrefService.h>
#include <nsIServiceManager.h>
#include <nsMemory.h>
#include <glib/gmessages.h>
#include <glib/gstrfuncs.h>

gboolean
mozilla_prefs_save (void)
{
        nsCOMPtr<nsIPrefService> prefService = 
                                 do_GetService (NS_PREFSERVICE_CONTRACTID);
        g_return_val_if_fail (prefService != nsnull, FALSE);

        nsresult rv = prefService->SavePrefFile (nsnull);
        return NS_SUCCEEDED (rv) ? TRUE : FALSE;
}

gboolean
mozilla_prefs_set_string(const char *preference_name, const char *new_value)
{
        g_return_val_if_fail (preference_name != NULL, FALSE);
        g_return_val_if_fail (new_value != NULL, FALSE);
        nsCOMPtr<nsIPrefService> prefService = 
                                do_GetService (NS_PREFSERVICE_CONTRACTID);
        nsCOMPtr<nsIPrefBranch> pref;
        prefService->GetBranch ("", getter_AddRefs(pref));

        if (pref)
        {
		nsresult rv = pref->SetCharPref (preference_name, new_value);            
                return NS_SUCCEEDED (rv) ? TRUE : FALSE;
        }

        return FALSE;
}

gboolean
mozilla_prefs_set_boolean (const char *preference_name,
                           gboolean new_boolean_value)
{
        g_return_val_if_fail (preference_name != NULL, FALSE);
  
        nsCOMPtr<nsIPrefService> prefService = 
                                do_GetService (NS_PREFSERVICE_CONTRACTID);
        nsCOMPtr<nsIPrefBranch> pref;
        prefService->GetBranch ("", getter_AddRefs(pref));
  
        if (pref)
        {
                nsresult rv = pref->SetBoolPref (preference_name,
                                new_boolean_value ? PR_TRUE : PR_FALSE);
                return NS_SUCCEEDED (rv) ? TRUE : FALSE;
        }
        return FALSE;
}

gboolean
mozilla_prefs_set_int (const char *preference_name, int new_int_value)
{
        g_return_val_if_fail (preference_name != NULL, FALSE);

        nsCOMPtr<nsIPrefService> prefService = 
                                do_GetService (NS_PREFSERVICE_CONTRACTID);
        nsCOMPtr<nsIPrefBranch> pref;
        prefService->GetBranch ("", getter_AddRefs(pref));

        if (pref)
        {
                nsresult rv = pref->SetIntPref (preference_name, new_int_value);
                return NS_SUCCEEDED (rv) ? TRUE : FALSE;
        }

        return FALSE;
}

gboolean
mozilla_prefs_get_boolean (const char *preference_name,
                           gboolean default_value)
{
        PRBool value;

        g_return_val_if_fail (preference_name != NULL, FALSE);

        nsCOMPtr<nsIPrefService> prefService = 
                                do_GetService (NS_PREFSERVICE_CONTRACTID);
        nsCOMPtr<nsIPrefBranch> pref;
        prefService->GetBranch ("", getter_AddRefs(pref));

        if (pref)
        {
                nsresult result;
                
                result = pref->GetBoolPref (preference_name, &value);
                if (NS_FAILED (result)) return default_value;
        }

        return (value == PR_TRUE) ? TRUE : FALSE;
}

gint
mozilla_prefs_get_int (const char *preference_name)
{
        int value = -1;

        g_return_val_if_fail (preference_name != NULL, FALSE);

        nsCOMPtr<nsIPrefService> prefService = 
                                do_GetService (NS_PREFSERVICE_CONTRACTID);
        nsCOMPtr<nsIPrefBranch> pref;
        prefService->GetBranch ("", getter_AddRefs(pref));

        if (pref)
        {
                pref->GetIntPref (preference_name, &value);
        }

        return value;
}

gchar *
mozilla_prefs_get_string(const char *preference_name)
{
        gchar *value = NULL;
        gchar *result = NULL;

        g_return_val_if_fail (preference_name != NULL, FALSE);
        nsCOMPtr<nsIPrefService> prefService = 
                                do_GetService (NS_PREFSERVICE_CONTRACTID);
        nsCOMPtr<nsIPrefBranch> pref;
        prefService->GetBranch ("", getter_AddRefs(pref));

        if (pref)
        {
                pref->GetCharPref (preference_name, &value);            
        }

        /* it's allocated by mozilla, so I could not g_free it */
        if (value)
        {
                result = g_strdup (value);
                nsMemory::Free (value);
        }

        return result;
}

gboolean
mozilla_prefs_remove (const char *preference_name)
{
        g_return_val_if_fail (preference_name != NULL, FALSE);

        nsCOMPtr<nsIPrefService> prefService = 
                                do_GetService (NS_PREFSERVICE_CONTRACTID);
        nsCOMPtr<nsIPrefBranch> pref;
        prefService->GetBranch ("", getter_AddRefs(pref));

        if (pref)
        {
                nsresult rv = pref->ClearUserPref (preference_name);
                return NS_SUCCEEDED (rv) ? TRUE : FALSE;
        }

        return FALSE;
}
