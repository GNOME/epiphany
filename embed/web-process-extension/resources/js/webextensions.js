'use strict';

/* exported pageActionOnClicked, browserActionClicked, browserActionClicked, tabsOnUpdated */
/* global ephy_message EphyEventListener */

// Browser async API
window.browser.alarms = {
    clearAll: function (args, cb) { return ephy_message ('alarms.clearAll', args, cb); },
};

window.browser.windows = {
    onRemoved: new EphyEventListener (),
};

window.browser.tabs = {
    create: function (args, cb) { return ephy_message ('tabs.create', args, cb); },
    executeScript: function (...args) { return ephy_message ('tabs.executeScript', args, null); },
    query: function (args, cb) { return ephy_message ('tabs.query', args, cb); },
    get: function (args, cb) { return ephy_message ('tabs.get', args, cb); },
    insertCSS: function (...args) { return ephy_message ('tabs.insertCSS', args, null); },
    removeCSS: function (...args) { return ephy_message ('tabs.removeCSS', args, null); },
    onUpdated: new EphyEventListener (),
    sendMessage: function (...args) { return ephy_message ('tabs.sendMessage', args, null); },
    TAB_ID_NONE: -1,
};

window.browser.notifications = {
    create: function (args, cb) { return ephy_message ('notifications.create', args, cb); },
};

// browser.extension is defined in ephy-webextension-common.c
window.browser.extension.getViews = function (...args) { return []; };

// browser.runtime is defined in webextensions-common.js
window.browser.runtime.getBrowserInfo = function (args, cb) { return ephy_message ('runtime.getBrowserInfo', args, cb); };
window.browser.runtime.connectNative = function (args, cb) { return ephy_message ('runtime.connectNative', args, cb); };
window.browser.runtime.openOptionsPage = function (args, cb) { return ephy_message ('runtime.openOptionsPage', args, cb); };
window.browser.runtime.setUninstallURL = function (args, cb) { return ephy_message ('runtime.setUninstallURL', args, cb); };
window.browser.runtime.onInstalled = new EphyEventListener ();
window.browser.runtime.onMessageExternal = new EphyEventListener ();
window.browser.runtime.sendNativeMessage = function (args) {
  return new Promise ((resolve, reject) => { reject ('Unsupported API'); });
};

window.browser.pageAction = {
    setIcon: function (args, cb) { return ephy_message ('pageAction.setIcon', args, cb); },
    setTitle: function (args, cb) { return ephy_message ('pageAction.setTitle', args, cb); },
    getTitle: function (args, cb) { return ephy_message ('pageAction.getTitle', args, cb); },
    show: function (args, cb) { return ephy_message ('pageAction.show', args, cb); },
    hide: function (args, cb) { return ephy_message ('pageAction.hide', args, cb); },
    onClicked: new EphyEventListener (),
};

window.browser.browserAction = {
    onClicked: new EphyEventListener (),
};

window.browser.windows = {
  WINDOW_ID_CURRENT: -2, /* Matches Firefox, used in tabs.c. */
};

/* Firefox returns null in private mode. So extensions sometimes handle this. */
window.browser.extension.getBackgroundPage = function () { return null; };
