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

#### notifications

Limitations:

- Icons are not supported.
- We can't track when notificaions are shown or dismissed.

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

There are roughly 3 contexts in which WebExtension code executes in that need to be handled properly:

#### Content Scripts

The full details can be found here: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Content_scripts

These run inside of the main web views context and have access to the DOM however they are in
a private ScriptWorld and cannot directly interact with other extensions or the websites JavaScript.

They also only have access to a small subset of the WebExtensions API.

#### Action Views

These are WebExtension views created when the user triggers an action. They are short lived and limited in presentation but have
full access to the WebExtension APIs.

Since they are isolated from the website they run their scripts in its default context.

#### Background Page

This is similar to an Action view except that is a long-lived view, Action Views can't get direct access with it, and Content Scripts can communicate with it.
