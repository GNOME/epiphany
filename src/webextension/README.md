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
- WebExtension API:
    - i18n:
        - getMessage
        - getUILanguage
    - runtime:
        - sendMessage
        - onMessage.addListener
    - notifications:
        - create
    - pageAction:
        - setIcon
        - setTitle
        - show
    - getTitle
    - tabs:
        - insertCSS
        - removeCSS
        - initial query

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
