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
  EPHY_PREFS_READER_FONT_STYLE_SANS,
  EPHY_PREFS_READER_FONT_STYLE_SERIF,
} EphyPrefsReaderFontStyle;

typedef enum
{
  EPHY_PREFS_READER_COLORS_LIGHT,
  EPHY_PREFS_READER_COLORS_DARK,
} EphyPrefsReaderColorScheme;

typedef enum
{
  EPHY_PREFS_RESTORE_SESSION_POLICY_ALWAYS,
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
  EPHY_PREFS_WEB_HARDWARE_ACCELERATION_POLICY_ALWAYS,
  EPHY_PREFS_WEB_HARDWARE_ACCELERATION_POLICY_NEVER
} EphyPrefsWebHardwareAccelerationPolicy;

#define EPHY_PREFS_UI_SCHEMA                     "org.gnome.Epiphany.ui"
#define EPHY_PREFS_UI_EXPAND_TABS_BAR            "expand-tabs-bar"
#define EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY "tabs-bar-visibility-policy"
#define EPHY_PREFS_UI_KEEP_WINDOW_OPEN           "keep-window-open"
#define EPHY_PREFS_UI_BOTTOM_URL_BAR             "bottom-url-bar"
#define EPHY_PREFS_UI_WEBKIT_FEATURES_PANEL      "webkit-features-page"
#define EPHY_PREFS_UI_WEBKIT_INTERNAL_FEATURES   "webkit-features-page-show-internal"

#define EPHY_PREFS_READER_SCHEMA                 "org.gnome.Epiphany.reader"
#define EPHY_PREFS_READER_FONT_STYLE             "font-style"
#define EPHY_PREFS_READER_COLOR_SCHEME           "color-scheme"

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

#define EPHY_PREFS_WEB_SCHEMA                       "org.gnome.Epiphany.web"
#define EPHY_PREFS_WEB_FONT_MIN_SIZE                "min-font-size"
#define EPHY_PREFS_WEB_LANGUAGE                     "language"
#define EPHY_PREFS_WEB_USE_GNOME_FONTS              "use-gnome-fonts"
#define EPHY_PREFS_WEB_SANS_SERIF_FONT              "sans-serif-font"
#define EPHY_PREFS_WEB_SERIF_FONT                   "serif-font"
#define EPHY_PREFS_WEB_MONOSPACE_FONT               "monospace-font"
#define EPHY_PREFS_WEB_ENABLE_USER_CSS              "enable-user-css"
#define EPHY_PREFS_WEB_ENABLE_USER_JS               "enable-user-js"
#define EPHY_PREFS_WEB_ENABLE_POPUPS                "enable-popups"
#define EPHY_PREFS_WEB_ENABLE_SPELL_CHECKING        "enable-spell-checking"
#define EPHY_PREFS_WEB_USER_AGENT                   "user-agent"
#define EPHY_PREFS_WEB_DEFAULT_ENCODING             "default-encoding"
#define EPHY_PREFS_WEB_ENABLE_ADBLOCK               "enable-adblock"
#define EPHY_PREFS_WEB_REMEMBER_PASSWORDS           "remember-passwords"
#define EPHY_PREFS_WEB_ENABLE_SITE_SPECIFIC_QUIRKS  "enable-site-specific-quirks"
#define EPHY_PREFS_WEB_ENABLE_ITP                   "enable-itp"
#define EPHY_PREFS_WEB_ENABLE_WEBSITE_DATA_STORAGE  "enable-website-data-storage"
#define EPHY_PREFS_WEB_DEFAULT_ZOOM_LEVEL           "default-zoom-level"
#define EPHY_PREFS_WEB_ENABLE_AUTOSEARCH            "enable-autosearch"
#define EPHY_PREFS_WEB_ENABLE_MOUSE_GESTURES        "enable-mouse-gestures"
#define EPHY_PREFS_WEB_LAST_UPLOAD_DIRECTORY        "last-upload-directory"
#define EPHY_PREFS_WEB_LAST_DOWNLOAD_DIRECTORY      "last-download-directory"
#define EPHY_PREFS_WEB_HARDWARE_ACCELERATION_POLICY "hardware-acceleration-policy"
#define EPHY_PREFS_WEB_ASK_ON_DOWNLOAD              "ask-on-download"
#define EPHY_PREFS_WEB_SWITCH_TO_NEW_TAB            "switch-to-new-tab"
#define EPHY_PREFS_WEB_ENABLE_WEBEXTENSIONS         "enable-webextensions"
#define EPHY_PREFS_WEB_WEBEXTENSIONS_ACTIVE         "webextensions-active"
#define EPHY_PREFS_WEB_SHOW_DEVELOPER_ACTIONS       "show-developer-actions"
#define EPHY_PREFS_WEB_ALWAYS_SHOW_FULL_URL         "always-show-full-url"
#define EPHY_PREFS_WEB_ENABLE_NAVIGATION_GESTURES   "enable-navigation-gestures"
#define EPHY_PREFS_WEB_AUTOFILL_DATA                "autofill-data"
#define EPHY_PREFS_WEB_AUTO_OPEN_SCHEMES            "auto-open-schemes"
#define EPHY_PREFS_WEB_READER_MODE_ZOOM_LEVEL       "reader-mode-zoom-level"
#define EPHY_PREFS_WEB_ENABLE_COOKIE_BANNER         "enable-cookie-banner"

static const char * const ephy_prefs_web_schema[] = {
  EPHY_PREFS_WEB_FONT_MIN_SIZE,
  EPHY_PREFS_WEB_LANGUAGE,
  EPHY_PREFS_WEB_USE_GNOME_FONTS,
  EPHY_PREFS_WEB_SANS_SERIF_FONT,
  EPHY_PREFS_WEB_SERIF_FONT,
  EPHY_PREFS_WEB_MONOSPACE_FONT,
  EPHY_PREFS_WEB_ENABLE_USER_CSS,
  EPHY_PREFS_WEB_ENABLE_USER_JS,
  EPHY_PREFS_WEB_ENABLE_POPUPS,
  EPHY_PREFS_WEB_ENABLE_SPELL_CHECKING,
  EPHY_PREFS_WEB_USER_AGENT,
  EPHY_PREFS_WEB_DEFAULT_ENCODING,
  EPHY_PREFS_WEB_ENABLE_ADBLOCK,
  EPHY_PREFS_WEB_REMEMBER_PASSWORDS,
  EPHY_PREFS_WEB_ENABLE_SITE_SPECIFIC_QUIRKS,
  EPHY_PREFS_WEB_ENABLE_ITP,
  EPHY_PREFS_WEB_ENABLE_WEBSITE_DATA_STORAGE,
  EPHY_PREFS_WEB_DEFAULT_ZOOM_LEVEL,
  EPHY_PREFS_WEB_ENABLE_AUTOSEARCH,
  EPHY_PREFS_WEB_ENABLE_MOUSE_GESTURES,
  EPHY_PREFS_WEB_LAST_UPLOAD_DIRECTORY,
  EPHY_PREFS_WEB_LAST_DOWNLOAD_DIRECTORY,
  EPHY_PREFS_WEB_HARDWARE_ACCELERATION_POLICY,
  EPHY_PREFS_WEB_ASK_ON_DOWNLOAD,
  EPHY_PREFS_WEB_SWITCH_TO_NEW_TAB,
  EPHY_PREFS_WEB_ENABLE_WEBEXTENSIONS,
  EPHY_PREFS_WEB_SHOW_DEVELOPER_ACTIONS,
  EPHY_PREFS_WEB_ALWAYS_SHOW_FULL_URL,
  EPHY_PREFS_WEB_AUTOFILL_DATA,
  EPHY_PREFS_WEB_READER_MODE_ZOOM_LEVEL,
};

#define EPHY_PREFS_SCHEMA                             "org.gnome.Epiphany"
#define EPHY_PREFS_HOMEPAGE_URL                       "homepage-url"
#define EPHY_PREFS_NEW_WINDOWS_IN_TABS                "new-windows-in-tabs"
#define EPHY_PREFS_WARN_ON_CLOSE_UNSUBMITTED_DATA     "warn-on-close-unsubmitted-data"
#define EPHY_PREFS_ENABLE_CARET_BROWSING              "enable-caret-browsing"
#define EPHY_PREFS_RESTORE_SESSION_POLICY             "restore-session-policy"
#define EPHY_PREFS_RESTORE_SESSION_DELAYING_LOADS     "restore-session-delaying-loads"
#define EPHY_PREFS_CONTENT_FILTERS                    "content-filters"
#define EPHY_PREFS_SEARCH_ENGINES                     "search-engine-providers"
#define EPHY_PREFS_DEFAULT_SEARCH_ENGINE              "default-search-engine"
#define EPHY_PREFS_ASK_FOR_DEFAULT                    "ask-for-default"
#define EPHY_PREFS_START_IN_INCOGNITO_MODE            "start-in-incognito-mode"
#define EPHY_PREFS_ACTIVE_CLEAR_DATA_ITEMS            "active-clear-data-items"
#define EPHY_PREFS_INCOGNITO_SEARCH_ENGINE            "incognito-search-engine"
#define EPHY_PREFS_USE_SEARCH_SUGGESTIONS             "use-search-suggestions"

#define EPHY_PREFS_LOCKDOWN_SCHEMA            "org.gnome.Epiphany.lockdown"
#define EPHY_PREFS_LOCKDOWN_FULLSCREEN        "disable-fullscreen"
#define EPHY_PREFS_LOCKDOWN_ARBITRARY_URL     "disable-arbitrary-url"
#define EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING  "disable-bookmark-editing"
#define EPHY_PREFS_LOCKDOWN_HISTORY           "disable-history"
#define EPHY_PREFS_LOCKDOWN_SAVE_TO_DISK      "disable-save-to-disk"
#define EPHY_PREFS_LOCKDOWN_PRINTING          "disable-printing"
#define EPHY_PREFS_LOCKDOWN_QUIT              "disable-quit"
#define EPHY_PREFS_LOCKDOWN_CONTEXT_MENU      "disable-context-menu"

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
#define EPHY_PREFS_SYNC_TOKEN_SERVER      "sync-token-server"
#define EPHY_PREFS_SYNC_ACCOUNTS_SERVER   "sync-accounts-server"

#define EPHY_PREFS_WEB_APP_SCHEMA                  "org.gnome.Epiphany.webapp"
#define EPHY_PREFS_WEB_APP_ADDITIONAL_URLS         "additional-urls"
#define EPHY_PREFS_WEB_APP_SHOW_NAVIGATION_BUTTONS "show-navigation-buttons"
#define EPHY_PREFS_WEB_APP_RUN_IN_BACKGROUND       "run-in-background"
#define EPHY_PREFS_WEB_APP_SYSTEM                  "system"

static struct {
  const char *schema;
  const char *path;
  gboolean is_webapp_only;
} const ephy_prefs_relocatable_schemas[] = {
  { EPHY_PREFS_STATE_SCHEMA, "state/", FALSE },
  { EPHY_PREFS_WEB_SCHEMA, "web/", FALSE },
  { EPHY_PREFS_WEB_APP_SCHEMA, "webapp/", TRUE }
};

G_END_DECLS
