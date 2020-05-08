#pragma once

#include <glib-object.h>

#define HANDY_USE_UNSTABLE_API
#include <handy.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PREFS_APPEARANCE_PAGE (prefs_appearance_page_get_type ())

G_DECLARE_FINAL_TYPE (PrefsAppearancePage, prefs_appearance_page, EPHY, PREFS_APPEARANCE_PAGE, HdyPreferencesPage)

G_END_DECLS
