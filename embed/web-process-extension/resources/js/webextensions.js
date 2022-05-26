'use strict';

/* exported pageActionOnClicked, browserActionClicked, browserActionClicked, tabsOnUpdated */
/* global ephy_message EphyEventListener */

// Browser async API
window.browser.alarms = {
    clearAll: function (...args) { return ephy_message ('alarms.clearAll', args); },
};

window.browser.windows = {
    onRemoved: new EphyEventListener (),
};

window.browser.tabs = {
    create: function (...args) { return ephy_message ('tabs.create', args); },
    executeScript: function (...args) { return ephy_message ('tabs.executeScript', args); },
    query: function (...args) { return ephy_message ('tabs.query', args); },
    get: function (...args) { return ephy_message ('tabs.get', args); },
    insertCSS: function (...args) { return ephy_message ('tabs.insertCSS', args); },
    removeCSS: function (...args) { return ephy_message ('tabs.removeCSS', args); },
    onUpdated: new EphyEventListener (),
    sendMessage: function (...args) { return ephy_message ('tabs.sendMessage', args); },
    TAB_ID_NONE: -1,
};

window.browser.notifications = {
    create: function (...args) { return ephy_message ('notifications.create', args); },
};

// browser.extension is defined in ephy-webextension-common.c
window.browser.extension.getViews = function (...args) { return []; };
// Firefox returns null in private mode. So extensions sometimes handle this.
window.browser.extension.getBackgroundPage = function () { return null; };

// browser.runtime is defined in webextensions-common.js
window.browser.runtime.getBrowserInfo = function (...args) { return ephy_message ('runtime.getBrowserInfo', args); };
window.browser.runtime.connectNative = function (...args) { return ephy_message ('runtime.connectNative', args); };
window.browser.runtime.openOptionsPage = function (...args) { return ephy_message ('runtime.openOptionsPage', args); };
window.browser.runtime.setUninstallURL = function (...args) { return ephy_message ('runtime.setUninstallURL', args); };
window.browser.runtime.onInstalled = new EphyEventListener ();
window.browser.runtime.onMessageExternal = new EphyEventListener ();
window.browser.runtime.sendNativeMessage = function (...args) { return ephy_message ('runtime.sendNativeMessage', args); };


window.browser.pageAction = {
    setIcon: function (...args) { return ephy_message ('pageAction.setIcon', args); },
    setTitle: function (...args) { return ephy_message ('pageAction.setTitle', args); },
    getTitle: function (...args) { return ephy_message ('pageAction.getTitle', args); },
    show: function (...args) { return ephy_message ('pageAction.show', args); },
    hide: function (...args) { return ephy_message ('pageAction.hide', args); },
    onClicked: new EphyEventListener (),
};

window.browser.browserAction = {
    onClicked: new EphyEventListener (),
};

window.browser.windows = {
  WINDOW_ID_CURRENT: -2, /* Matches Firefox, used in tabs.c. */
};
