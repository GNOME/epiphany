https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API

https://github.com/mdn/webextensions-examples


# Working extensions

- Borderify
- Apply CSS
- Page to extension messaging

# QUESTIONS
 - Should we use **self** as current module parameter name for consistency or name it like module?
 - Clear definition if get/set functions should be used instead of direct struct access
 - Enfore g_auto free functions implementation?
 - Alignment in header files
 - Should every function of a file has a certain prefix or only non static functions?
 - EphyWebExtensionManager as a singleton?
 
# PLAN

## First release
Feature set:
 - Un/Load/Enable/Disable xpi and extracted extensions
 - Works for existing and new views
 - Manifest file:
    - initial content_scripts
    - initial background page
    - initial background scripts
 - API:
    - notifications:
        - create
    - pageaction:
        - setIcon
        - setTitle
        - show
        - getTitle
    - tabs:
        - insertCSS
        - removeCSS
        - initial query

 - Test extensions:
    - apply-css
    - borderify
    
## Second release
Feature set:
 - API:
    - i18n:
        - getMessage
        - getUILanguage
    - runtime:
        - sendMessage
        - onMessage.addListener

 - Test extensions:
    - notify-link-clicks-i18n

