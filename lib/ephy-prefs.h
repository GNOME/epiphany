/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2010 Igalia S.L.
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

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_PREFS_H
#define EPHY_PREFS_H

G_BEGIN_DECLS

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
  EPHY_PREFS_WEB_COOKIES_POLICY_NEVER
} EphyPrefsWebCookiesPolicy;

typedef enum
{
  EPHY_PREFS_PROCESS_MODEL_SHARED_SECONDARY_PROCESS,
  EPHY_PREFS_PROCESS_MODEL_ONE_SECONDARY_PROCESS_PER_WEB_VIEW
} EphyPrefsProcessModel;

#define EPHY_PREFS_UI_SCHEMA                     "org.gnome.Epiphany.ui"
#define EPHY_PREFS_UI_ALWAYS_SHOW_TABS_BAR       "always-show-tabs-bar"
#define EPHY_PREFS_UI_DOWNLOADS_HIDDEN           "downloads-hidden"
#define EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY "tabs-bar-visibility-policy"

#define EPHY_PREFS_STATE_SCHEMA                 "org.gnome.Epiphany.state"
#define EPHY_PREFS_STATE_SAVE_DIR               "save-dir"
#define EPHY_PREFS_STATE_SAVE_IMAGE_DIR         "save-image-dir"
#define EPHY_PREFS_STATE_OPEN_DIR               "open-dir"
#define EPHY_PREFS_STATE_DOWNLOAD_DIR           "download-dir"
#define EPHY_PREFS_STATE_UPLOAD_DIR             "upload-dir"
#define EPHY_PREFS_STATE_RECENT_ENCODINGS       "recent-encodings"
#define EPHY_PREFS_STATE_BOOKMARKS_VIEW_TITLE   "bookmarks-view-title"
#define EPHY_PREFS_STATE_BOOKMARKS_VIEW_ADDRESS "bookmarks-view-address"

#define EPHY_PREFS_WEB_SCHEMA                "org.gnome.Epiphany.web"
#define EPHY_PREFS_WEB_FONT_MIN_SIZE         "min-font-size"
#define EPHY_PREFS_WEB_LANGUAGE              "language"
#define EPHY_PREFS_WEB_USE_OWN_FONTS         "use-own-fonts"
#define EPHY_PREFS_WEB_USE_GNOME_FONTS       "use-gnome-fonts"
#define EPHY_PREFS_WEB_SANS_SERIF_FONT       "sans-serif-font"
#define EPHY_PREFS_WEB_SERIF_FONT            "serif-font"
#define EPHY_PREFS_WEB_MONOSPACE_FONT        "monospace-font"
#define EPHY_PREFS_WEB_USE_OWN_COLORS        "use-own-colors"
#define EPHY_PREFS_WEB_ENABLE_USER_CSS       "enable-user-css"
#define EPHY_PREFS_WEB_ENABLE_POPUPS         "enable-popups"
#define EPHY_PREFS_WEB_ENABLE_PLUGINS        "enable-plugins"
#define EPHY_PREFS_WEB_ENABLE_JAVASCRIPT     "enable-javascript"
#define EPHY_PREFS_WEB_ENABLE_SPELL_CHECKING "enable-spell-checking"
#define EPHY_PREFS_WEB_ENABLE_WEBGL          "enable-webgl"
#define EPHY_PREFS_WEB_ENABLE_WEBAUDIO       "enable-webaudio"
#define EPHY_PREFS_WEB_COOKIES_POLICY        "cookies-policy"
#define EPHY_PREFS_WEB_IMAGE_ANIMATION_MODE  "image-animation-mode"
#define EPHY_PREFS_WEB_DEFAULT_ENCODING      "default-encoding"
#define EPHY_PREFS_WEB_DO_NOT_TRACK          "do-not-track"
#define EPHY_PREFS_WEB_ENABLE_ADBLOCK        "enable-adblock"

#define EPHY_PREFS_SCHEMA                         "org.gnome.Epiphany"
#define EPHY_PREFS_USER_AGENT                     "user-agent"
#define EPHY_PREFS_NEW_WINDOWS_IN_TABS            "new-windows-in-tabs"
#define EPHY_PREFS_AUTO_DOWNLOADS                 "automatic-downloads"
#define EPHY_PREFS_WARN_ON_CLOSE_UNSUBMITTED_DATA "warn-on-close-unsubmitted-data"
#define EPHY_PREFS_REMEMBER_PASSWORDS             "remember-passwords"
#define EPHY_PREFS_KEYWORD_SEARCH_URL             "keyword-search-url"
#define EPHY_PREFS_MANAGED_NETWORK                "managed-network"
#define EPHY_PREFS_ENABLE_SMOOTH_SCROLLING        "enable-smooth-scrolling"
#define EPHY_PREFS_ENABLE_CARET_BROWSING          "enable-caret-browsing"
#define EPHY_PREFS_INTERNAL_VIEW_SOURCE           "internal-view-source"
#define EPHY_PREFS_RESTORE_SESSION_POLICY         "restore-session-policy"
#define EPHY_PREFS_RESTORE_SESSION_DELAYING_LOADS "restore-session-delaying-loads"
#define EPHY_PREFS_PROCESS_MODEL                  "process-model"
#define EPHY_PREFS_MAX_PROCESSES                  "max-processes"

#define EPHY_PREFS_LOCKDOWN_SCHEMA            "org.gnome.Epiphany.lockdown"
#define EPHY_PREFS_LOCKDOWN_FULLSCREEN        "disable-fullscreen"
#define EPHY_PREFS_LOCKDOWN_ARBITRARY_URL     "disable-arbitrary-url"
#define EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING  "disable-bookmark-editing"
#define EPHY_PREFS_LOCKDOWN_HISTORY           "disable-history"
#define EPHY_PREFS_LOCKDOWN_SAVE_TO_DISK      "disable-save-to-disk"
#define EPHY_PREFS_LOCKDOWN_PRINTING          "disable-printing"
#define EPHY_PREFS_LOCKDOWN_PRINT_SETUP       "disable-print-setup"
#define EPHY_PREFS_LOCKDOWN_COMMAND_LINE      "disable-command-line"
#define EPHY_PREFS_LOCKDOWN_QUIT              "disable-quit"
#define EPHY_PREFS_LOCKDOWN_JAVASCRIPT_CHROME "disable-javascript-chrome"

G_END_DECLS

#endif
