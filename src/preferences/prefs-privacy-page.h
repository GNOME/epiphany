#pragma once

#include <glib-object.h>

#define HANDY_USE_UNSTABLE_API
#include <handy.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PREFS_PRIVACY_PAGE (prefs_privacy_page_get_type ())

G_DECLARE_FINAL_TYPE (PrefsPrivacyPage, prefs_privacy_page, EPHY, PREFS_PRIVACY_PAGE, HdyPreferencesPage)

G_END_DECLS
