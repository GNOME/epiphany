#pragma once

#include <glib-object.h>

#define HANDY_USE_UNSTABLE_API
#include <handy.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PREFS_SYNC_PAGE (prefs_sync_page_get_type ())

G_DECLARE_FINAL_TYPE (PrefsSyncPage, prefs_sync_page, EPHY, PREFS_SYNC_PAGE, HdyPreferencesPage)

void prefs_sync_page_setup (PrefsSyncPage *sync_page);

G_END_DECLS
