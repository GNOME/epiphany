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
	EPHY_PREFS_UI_TOOLBAR_STYLE_BOTH,
	EPHY_PREFS_UI_TOOLBAR_STYLE_BOTH_HORIZ,
	EPHY_PREFS_UI_TOOLBAR_STYLE_BOTH_ICONS,
	EPHY_PREFS_UI_TOOLBAR_STYLE_BOTH_TEXT
} EphyPrefsUIToolbarStyle;

typedef enum
{
	EPHY_PREFS_WEB_COOKIES_POLICY_ALWAYS,
	EPHY_PREFS_WEB_COOKIES_POLICY_NO_THIRD_PARTY,
	EPHY_PREFS_WEB_COOKIES_POLICY_NEVER
} EphyPrefsWebCookiesPolicy;

typedef enum
{
	EPHY_PREFS_STATE_HISTORY_DATE_FILTER_LAST_HALF_HOUR,
	EPHY_PREFS_STATE_HISTORY_DATE_FILTER_EVER,
	EPHY_PREFS_STATE_HISTORY_DATE_FILTER_TODAY,
	EPHY_PREFS_STATE_HISTORY_DATE_FILTER_LAST_TWO_DAYS,
	EPHY_PREFS_STATE_HISTORY_DATE_FILTER_LAST_THREE_DAYS
} EphyPrefsStateHistoryDateFilter;

#define EPHY_PREFS_UI_SCHEMA			"org.gnome.Epiphany.ui"
#define EPHY_PREFS_UI_ALWAYS_SHOW_TABS_BAR	"always-show-tabs-bar"
#define EPHY_PREFS_UI_SHOW_TOOLBARS		"show-toolbars"
#define EPHY_PREFS_UI_SHOW_BOOKMARKS_BAR	"show-bookmarks-bar"
#define EPHY_PREFS_UI_TOOLBAR_STYLE		"toolbar-style"
#define EPHY_PREFS_UI_DOWNLOADS_HIDDEN		"downloads-hidden"

#define EPHY_PREFS_STATE_SCHEMA			"org.gnome.Epiphany.state"
#define EPHY_PREFS_STATE_SAVE_DIR		"save-dir"
#define EPHY_PREFS_STATE_SAVE_IMAGE_DIR		"save-image-dir"
#define EPHY_PREFS_STATE_OPEN_DIR		"open-dir"
#define EPHY_PREFS_STATE_DOWNLOAD_DIR		"download-dir"
#define EPHY_PREFS_STATE_UPLOAD_DIR		"upload-dir"
#define EPHY_PREFS_STATE_RECENT_ENCODINGS	"recent-encodings"
#define EPHY_PREFS_STATE_BOOKMARKS_VIEW_TITLE	"bookmarks-view-title"
#define EPHY_PREFS_STATE_BOOKMARKS_VIEW_ADDRESS	"bookmarks-view-address"
#define EPHY_PREFS_STATE_HISTORY_VIEW_TITLE	"history-view-title"
#define EPHY_PREFS_STATE_HISTORY_VIEW_ADDRESS	"history-view-address"
#define EPHY_PREFS_STATE_HISTORY_VIEW_DATE	"history-view-date"
#define EPHY_PREFS_STATE_HISTORY_DATE_FILTER	"history-date-filter"

#define EPHY_PREFS_WEB_SCHEMA			"org.gnome.Epiphany.web"
#define EPHY_PREFS_WEB_FONT_MIN_SIZE		"min-font-size"
#define EPHY_PREFS_WEB_LANGUAGE			"language"
#define EPHY_PREFS_WEB_USE_OWN_FONTS		"use-own-fonts"
#define EPHY_PREFS_WEB_USE_OWN_COLORS		"use-own-colors"
#define EPHY_PREFS_WEB_ENABLE_USER_CSS		"enable-user-css"
#define EPHY_PREFS_WEB_ENABLE_POPUPS		"enable-popups"
#define EPHY_PREFS_WEB_ENABLE_PLUGINS		"enable-plugins"
#define EPHY_PREFS_WEB_ENABLE_JAVASCRIPT	"enable-javascript"
#define EPHY_PREFS_WEB_COOKIES_POLICY		"cookies-policy"
#define EPHY_PREFS_WEB_IMAGE_ANIMATION_MODE	"image-animation-mode"
#define EPHY_PREFS_WEB_DEFAULT_ENCODING		"default-encoding"

#define EPHY_PREFS_SCHEMA			"org.gnome.Epiphany"
#define EPHY_PREFS_HOMEPAGE_URL			"homepage-url"
#define EPHY_PREFS_USER_AGENT			"user-agent"
#define EPHY_PREFS_CACHE_SIZE			"cache-size"
#define EPHY_PREFS_NEW_WINDOWS_IN_TABS		"new-windows-in-tabs"
#define EPHY_PREFS_AUTO_DOWNLOADS		"automatic-downloads"
#define EPHY_PREFS_WARN_ON_CLOSE_UNSUBMITTED_DATA	"warn-on-close-unsubmitted-data"
#define EPHY_PREFS_MIDDLE_CLICK_OPENS_URL	"middle-click-opens-url"
#define EPHY_PREFS_REMEMBER_PASSWORDS		"remember-passwords"
#define EPHY_PREFS_KEYWORD_SEARCH_URL		"keyword-search-url"
#define EPHY_PREFS_MANAGED_NETWORK		"managed-network"
#define EPHY_PREFS_ENABLE_SMOOTH_SCROLLING	"enable-smooth-scrolling"
#define EPHY_PREFS_ENABLE_CARET_BROWSING	"enable-caret-browsing"
#define EPHY_PREFS_ENABLED_EXTENSIONS		"enabled-extensions"

#define EPHY_PREFS_LOCKDOWN_SCHEMA		"org.gnome.Epiphany.lockdown"
#define EPHY_PREFS_LOCKDOWN_FULLSCREEN		"disable-fullscreen"
#define EPHY_PREFS_LOCKDOWN_ARBITRARY_URL	"disable-arbitrary-url"
#define EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING	"disable-bookmark-editing"
#define EPHY_PREFS_LOCKDOWN_TOOLBAR_EDITING	"disable-toolbar-editing"
#define EPHY_PREFS_LOCKDOWN_HISTORY		"disable-history"
#define EPHY_PREFS_LOCKDOWN_SAVE_TO_DISK	"disable-save-to-disk"
#define EPHY_PREFS_LOCKDOWN_PRINTING		"disable-printing"
#define EPHY_PREFS_LOCKDOWN_PRINT_SETUP		"disable-print-setup"
#define EPHY_PREFS_LOCKDOWN_COMMAND_LINE	"disable-command-line"
#define EPHY_PREFS_LOCKDOWN_QUIT		"disable-quit"
#define EPHY_PREFS_LOCKDOWN_JAVASCRIPT_CHROME	"disable-javascript-chrome"
#define EPHY_PREFS_LOCKDOWN_MENUBAR		"disable-menubar"

G_END_DECLS

#endif
