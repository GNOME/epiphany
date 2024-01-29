# Epiphany WebExtensions

This is an experimental implementation of [WebExtensions](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API).

Some examples to run can be found here: https://github.com/mdn/webextensions-examples


It is a work in progress and should be assumed insecure, incomplete, and full of issues for now.

You can track issues here: https://gitlab.gnome.org/GNOME/epiphany/-/issues?label_name[]=5.+WebExtensions

## Feature Set

- Un/Load/Enable/Disable xpi and extracted extensions
- Works for existing and new views
- Manifest file:
    - initial content_scripts
    - initial background page
    - initial background scripts

### WebExtension JavaScript APIs

All of these APIs follow Firefox in behavior. They have the entrypoint of `browser` and return a `Promise`.

However just like Firefox we try to retain compatibility with Chrome and the `chrome` entry point works and if the last argument is a function it is treated as a callback with `lastError` set appropriately.

#### alarms

The alarms API is fully implemented.

- clear()
- clearAll()
- create()
- get()
- getAll()
- onAlarm

#### browserAction

- onClicked

#### cookies

Limitations:

- Epiphany only has a single cookieStore and all APIs will always use it.
- Filtering by firstParty domains isn't supported.
- You cannot request all cookies, a domain must be given.
- onChanged is only a stub and will never be called.

APIs:

- get()
- getAll()
- getAllCookieStores()
- set()
- remove()
- onChanged

#### downloads

Limitations:

- Unlike other browsers downloadIds are not persistent across sessions. Downloads are always "incognito" as they are always lost.
- Pausing/Resuming downloads is not supported.
- Returned download lists are not sorted by "orderBy" (TODO)

APIs:

- cancel()
- download()
- erase()
- open()
- removeFile()
- show()
- showDefaultFolder()
- search()
- onCreated
- onChanged
- onErased

#### extension

- getViews()
- getBackgroundPage()
- getURL()

#### i18n

This is only partially implemented, see https://gitlab.gnome.org/GNOME/epiphany/-/issues/1791

- getMessage()
- getUILanguage()

#### runtime

This is partially implemented. Message passing including replies works including from Content Scripts.

- getBackgroundPage()
- getBrowserInfo()
- getPlatformInfo()
- getURL()
- openOptionsPage()
- sendMessage()
- onMessage
- lastError

#### menus

Notes:

- `contextMenus` is aliased to this API and the same permissions work for both.

Limitations:

- Menus are only shown on web pages and not extension views, tabs, etc. (TODO)
- Radio and Checkbox menus aren't properly supported yet. (TODO)
- Commands are not yet supported. (TODO)
- update() isn't supported yet. (TODO)
- Frames are not properly handled.
- Icons are not supported.

APIs:

- create()
- remove()
- removeAll()
- onClicked

#### notifications

Limitations:

- Icons are not supported.
- We can't track when notifications are shown or dismissed.

APIs:

- create()
- clear()
- getAll()
- update()
- onClicked
- onButtonClicked

#### pageAction

- setIcon()
- setTitle()
- getTitle()
- show()
- onClicked

#### storage

Limitations:

- Sync storage isn't implemented but possible as Epiphany uses Firefox Sync.
- onChanged isn't implemented (TODO)

APIs:

- local.set()
- local.get()
- local.remove()
- local.clear()

#### tabs

Limitations:

- Some of these APIs are supposed to return after the page is loaded but return immediately. (TODO)
- None of the events are supported yet. (TODO)

APIs:

- create()
- query()
- insertCSS()
- remove()
- removeCSS()
- get()
- getZoom()
- setZoom()
- update()
- reload()
- executeScript()
- sendMessage()

#### windows

- get()
- getCurrent()
- getLastFocused()
- getAll()
- create()
- remove()
- onCreated
- onRemoved

### Tested extensions

- apply-css
- borderify
- notify-link-clicks-i18n

## Overview

### Contexts

Extensions generally have content running in their own private pages. That is pages under the `ephy-webextension:` URL scheme.
These pages have full access to the WebExtension API available in the default JavaScriptWorld.

Extensions can also inject [Content Scripts](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Content_scripts)
which run in a private JavaScriptWorld per-extension on each page it has permission to.

## How to add API

Adding a new API involves touching a few areas.

- `embed/web-process-extension/resources/js/webextensions-common.js` and `embed/web-process-extension/resources/js/webextensions.js` These files contain JavaScript injected into pages. The former is injected for Content Scripts and Extension Pages while the latter is only injected into Extension Pages. They do little more than expose the API.

- `embed/web-process-extension/ephy-webextension-api.c` This is a WebKitWebProcessExtension that extends Extension Page web views. This defines some
JavaScript API in C.

- `embed/web-process-extension/ephy-web-process-extension.c` This is a WebKitWebProcessExtension that extends normal web pages. It sets up the
private JavaScriptWorlds used for Content Scripts.

- `embed/web-process-extension/ephy-webextension-common.c` This is shared between the two WebKitWebProcessExtensions above. It is the bulk of the
`extension`/`runtime` and `i18n` APIs. It also handles much of the message passing to the UI process described below.

- `src/webextension/ephy-web-extension-manager.c` This is where all extensions are managed in the UI process and also where all messages from web views are
handled; In `content_scripts_handle_user_message()` and `extension_view_handle_user_message()` specifically with `api_handlers` being defined at the top of the file. These call out to the real API handlers.
This is also where a lot of event handling happens. The manager will listen to signals throughout Epiphany and call `ephy_web_extension_manager_emit_in_extension_views()`
to trigger events.

- `src/webextension/api/*` This is where all actual API handlers are implemented. A handler is given the source of the call (the WebExtension, its WebKitWebView), the name
of the call, the arguments (which were serialized over JSON), and a GTask to respond to the call.

NOTE: Please use `menus.c`/`menus.h` as an example starting point. The handlers have evolved to be written in a few styles with this being the direction we want to go in. It uses
only asynchronous API handlers and uses `json-glib` for all JSON parsing.

Be very careful about parsing the JSON arguments to be defensive against invalid values. The funcitons in `ephy-json-utils.h` help you by being strict on expected types.

Every API handler must return either an error with `g_task_return_new_error (task, WEB_EXTENSION_ERROR, ..., "foo.function(): Helpful reason");` or return a valid JSON
string with `g_task_return_pointer (task, allocated_json_string, g_free);`. `JsonBuilder` is helpful for building complex response objects or a simple `g_strdup ("true");`
works for some APIs. Many APIs require an empty response on success which is just `g_task_return_pointer (task, NULL, NULL);`. Your handler is entirely asynchronous and you
can store the GTask for later.
