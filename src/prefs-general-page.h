#pragma once

#include <glib-object.h>

#define HANDY_USE_UNSTABLE_API
#include <handy.h>

#include "prefs-dialog.h"

G_BEGIN_DECLS

#define EPHY_TYPE_PREFS_GENERAL_PAGE (prefs_general_page_get_type ())

G_DECLARE_FINAL_TYPE (PrefsGeneralPage, prefs_general_page, EPHY, PREFS_GENERAL_PAGE, HdyPreferencesPage)

void prefs_general_page_connect_pd_response (PrefsGeneralPage *general_page,
                                             PrefsDialog      *pd);

G_END_DECLS
