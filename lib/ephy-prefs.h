/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2010 Igalia S.L.
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

G_BEGIN_DECLS

typedef enum
{
  EPHY_PREFS_UI_TABS_BAR_POSITION_TOP,
  EPHY_PREFS_UI_TABS_BAR_POSITION_BOTTOM,
  EPHY_PREFS_UI_TABS_BAR_POSITION_LEFT,
  EPHY_PREFS_UI_TABS_BAR_POSITION_RIGHT
} EphyPrefsUITabsBarPosition;

typedef enum
{
  EPHY_PREFS_RESTORE_SESSION_POLICY_ALWAYS,
  EPHY_PREFS_RESTORE_SESSION_POLICY_NEVER,
  EPHY_PREFS_RESTORE_SESSION_POLICY_CRASHED
} EphyPrefsRestoreSessionPolicy;

typedef enum
{
  EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY_ALWAYS,
  EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY_MORE_THAN_ONE,
  EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY_NEVER
} EphyPrefsUITabsBarVisibilityPolicy;

typedef enum
{
  EPHY_PREFS_WEB_COOKIES_POLICY_ALWAYS,
  EPHY_PREFS_WEB_COOKIES_POLICY_NO_THIRD_PARTY,
  EPHY_PREFS_WEB_COOKIES_POLICY_NEVER,
  EPHY_PREFS_WEB_COOKIES_POLICY_INTELLIGENT_TRACKING_PREVENTION
} EphyPrefsWebCookiesPolicy;

typedef enum
{
  EPHY_PREFS_PROCESS_MODEL_SHARED_SECONDARY_PROCESS,
  EPHY_PREFS_PROCESS_MODEL_ONE_SECONDARY_PROCESS_PER_WEB_VIEW
} EphyPrefsProcessModel;

#define EPHY_PREFS_UI_SCHEMA                     "org.gnome.Epiphany.ui"
#define EPHY_PREFS_UI_EXPAND_TABS_BAR            "expand-tabs-bar"
#define EPHY_PREFS_UI_TABS_BAR_POSITION          "tabs-bar-position"
#define EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY "tabs-bar-visibility-policy"

#define EPHY_PREFS_STATE_SCHEMA                 "org.gnome.Epiphany.state"
#define EPHY_PREFS_STATE_DOWNLOAD_DIR           "download-dir"
#define EPHY_PREFS_STATE_RECENT_ENCODINGS       "recent-encodings"
#define EPHY_PREFS_STATE_WINDOW_POSITION        "window-position"
#define EPHY_PREFS_STATE_WINDOW_SIZE            "window-size"
#define EPHY_PREFS_STATE_IS_MAXIMIZED           "is-maximized"

static const char * const ephy_prefs_state_schema[] = {
  EPHY_PREFS_STATE_DOWNLOAD_DIR,
  EPHY_PREFS_STATE_RECENT_ENCODINGS,
  EPHY_PREFS_STATE_WINDOW_POSITION,
  EPHY_PREFS_STATE_WINDOW_SIZE,
  EPHY_PREFS_STATE_IS_MAXIMIZED
};

#define EPHY_PREFS_WEB_SCHEMA                      "org.gnome.Epiphany.web"
#define EPHY_PREFS_WEB_FONT_MIN_SIZE               "min-font-size"
#define EPHY_PREFS_WEB_LANGUAGE                    "language"
#define EPHY_PREFS_WEB_USE_GNOME_FONTS             "use-gnome-fonts"
#define EPHY_PREFS_WEB_SANS_SERIF_FONT             "sans-serif-font"
#define EPHY_PREFS_WEB_SERIF_FONT                  "serif-font"
#define EPHY_PREFS_WEB_MONOSPACE_FONT              "monospace-font"
#define EPHY_PREFS_WEB_ENABLE_USER_CSS             "enable-user-css"
#define EPHY_PREFS_WEB_ENABLE_POPUPS               "enable-popups"
#define EPHY_PREFS_WEB_ENABLE_PLUGINS              "enable-plugins"
#define EPHY_PREFS_WEB_ENABLE_SPELL_CHECKING       "enable-spell-checking"
#define EPHY_PREFS_WEB_ENABLE_WEBGL                "enable-webgl"
#define EPHY_PREFS_WEB_ENABLE_WEBAUDIO             "enable-webaudio"
#define EPHY_PREFS_WEB_ENABLE_SMOOTH_SCROLLING     "enable-smooth-scrolling"
#define EPHY_PREFS_WEB_USER_AGENT                  "user-agent"
#define EPHY_PREFS_WEB_COOKIES_POLICY              "cookies-policy"
#define EPHY_PREFS_WEB_DEFAULT_ENCODING            "default-encoding"
#define EPHY_PREFS_WEB_DO_NOT_TRACK                "do-not-track"
#define EPHY_PREFS_WEB_ENABLE_ADBLOCK              "enable-adblock"
#define EPHY_PREFS_WEB_REMEMBER_PASSWORDS          "remember-passwords"
#define EPHY_PREFS_WEB_ENABLE_SITE_SPECIFIC_QUIRKS "enable-site-specific-quirks"
#define EPHY_PREFS_WEB_ENABLE_SAFE_BROWSING        "enable-safe-browsing"
#define EPHY_PREFS_WEB_GSB_API_KEY                 "gsb-api-key"

static const char * const ephy_prefs_web_schema[] = {
  EPHY_PREFS_WEB_FONT_MIN_SIZE,
  EPHY_PREFS_WEB_LANGUAGE,
  EPHY_PREFS_WEB_USE_GNOME_FONTS,
  EPHY_PREFS_WEB_SANS_SERIF_FONT,
  EPHY_PREFS_WEB_SERIF_FONT,
  EPHY_PREFS_WEB_MONOSPACE_FONT,
  EPHY_PREFS_WEB_ENABLE_USER_CSS,
  EPHY_PREFS_WEB_ENABLE_POPUPS,
  EPHY_PREFS_WEB_ENABLE_PLUGINS,
  EPHY_PREFS_WEB_ENABLE_SPELL_CHECKING,
  EPHY_PREFS_WEB_ENABLE_WEBGL,
  EPHY_PREFS_WEB_ENABLE_WEBAUDIO,
  EPHY_PREFS_WEB_ENABLE_SMOOTH_SCROLLING,
  EPHY_PREFS_WEB_USER_AGENT,
  EPHY_PREFS_WEB_COOKIES_POLICY,
  EPHY_PREFS_WEB_DEFAULT_ENCODING,
  EPHY_PREFS_WEB_DO_NOT_TRACK,
  EPHY_PREFS_WEB_ENABLE_ADBLOCK,
  EPHY_PREFS_WEB_REMEMBER_PASSWORDS,
  EPHY_PREFS_WEB_ENABLE_SITE_SPECIFIC_QUIRKS,
  EPHY_PREFS_WEB_ENABLE_SAFE_BROWSING,
  EPHY_PREFS_WEB_GSB_API_KEY
};

#define EPHY_PREFS_SCHEMA                             "org.gnome.Epiphany"
#define EPHY_PREFS_HOMEPAGE_URL                       "homepage-url"
#define EPHY_PREFS_DEPRECATED_USER_AGENT              "user-agent"
#define EPHY_PREFS_NEW_WINDOWS_IN_TABS                "new-windows-in-tabs"
#define EPHY_PREFS_AUTO_DOWNLOADS                     "automatic-downloads"
#define EPHY_PREFS_WARN_ON_CLOSE_UNSUBMITTED_DATA     "warn-on-close-unsubmitted-data"
#define EPHY_PREFS_DEPRECATED_REMEMBER_PASSWORDS      "remember-passwords"
#define EPHY_PREFS_KEYWORD_SEARCH_URL                 "keyword-search-url"
#define EPHY_PREFS_DEPRECATED_ENABLE_SMOOTH_SCROLLING "enable-smooth-scrolling"
#define EPHY_PREFS_ENABLE_CARET_BROWSING              "enable-caret-browsing"
#define EPHY_PREFS_INTERNAL_VIEW_SOURCE               "internal-view-source"
#define EPHY_PREFS_RESTORE_SESSION_POLICY             "restore-session-policy"
#define EPHY_PREFS_RESTORE_SESSION_DELAYING_LOADS     "restore-session-delaying-loads"
#define EPHY_PREFS_PROCESS_MODEL                      "process-model"
#define EPHY_PREFS_MAX_PROCESSES                      "max-processes"
#define EPHY_PREFS_ADBLOCK_FILTERS                    "adblock-filters"
#define EPHY_PREFS_SEARCH_ENGINES                     "search-engines"
#define EPHY_PREFS_DEFAULT_SEARCH_ENGINE              "default-search-engine"

#define EPHY_PREFS_LOCKDOWN_SCHEMA            "org.gnome.Epiphany.lockdown"
#define EPHY_PREFS_LOCKDOWN_FULLSCREEN        "disable-fullscreen"
#define EPHY_PREFS_LOCKDOWN_ARBITRARY_URL     "disable-arbitrary-url"
#define EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING  "disable-bookmark-editing"
#define EPHY_PREFS_LOCKDOWN_HISTORY           "disable-history"
#define EPHY_PREFS_LOCKDOWN_SAVE_TO_DISK      "disable-save-to-disk"
#define EPHY_PREFS_LOCKDOWN_PRINTING          "disable-printing"
#define EPHY_PREFS_LOCKDOWN_QUIT              "disable-quit"

#define EPHY_PREFS_SYNC_SCHEMA            "org.gnome.Epiphany.sync"
#define EPHY_PREFS_SYNC_USER              "sync-user"
#define EPHY_PREFS_SYNC_TIME              "sync-time"
#define EPHY_PREFS_SYNC_DEVICE_ID         "sync-device-id"
#define EPHY_PREFS_SYNC_DEVICE_NAME       "sync-device-name"
#define EPHY_PREFS_SYNC_FREQUENCY         "sync-frequency"
#define EPHY_PREFS_SYNC_WITH_FIREFOX      "sync-with-firefox"
#define EPHY_PREFS_SYNC_BOOKMARKS_ENABLED "sync-bookmarks-enabled"
#define EPHY_PREFS_SYNC_BOOKMARKS_TIME    "sync-bookmarks-time"
#define EPHY_PREFS_SYNC_BOOKMARKS_INITIAL "sync-bookmarks-initial"
#define EPHY_PREFS_SYNC_PASSWORDS_ENABLED "sync-passwords-enabled"
#define EPHY_PREFS_SYNC_PASSWORDS_TIME    "sync-passwords-time"
#define EPHY_PREFS_SYNC_PASSWORDS_INITIAL "sync-passwords-initial"
#define EPHY_PREFS_SYNC_HISTORY_ENABLED   "sync-history-enabled"
#define EPHY_PREFS_SYNC_HISTORY_TIME      "sync-history-time"
#define EPHY_PREFS_SYNC_HISTORY_INITIAL   "sync-history-initial"
#define EPHY_PREFS_SYNC_OPEN_TABS_ENABLED "sync-open-tabs-enabled"
#define EPHY_PREFS_SYNC_OPEN_TABS_TIME    "sync-open-tabs-time"

static struct {
  const char *schema;
  const char *path;
} const ephy_prefs_relocatable_schemas[] = {
  { EPHY_PREFS_STATE_SCHEMA, "state/" },
  { EPHY_PREFS_WEB_SCHEMA, "web/" },
};

G_END_DECLS
