'use strict';

/* exported pageActionOnClicked, browserActionClicked, browserActionClicked, tabsOnUpdated */
/* global ephy_message */

const tabs_listeners = [];
const page_listeners = [];
const browser_listeners = [];
const runtime_listeners = [];
const runtime_onmessageexternal_listeners = [];
const windows_onremoved_listeners = [];

const pageActionOnClicked = function(x) {
  for (const listener of page_listeners)
    listener.callback(x);
};

const browserActionClicked = function(x) {
  for (const listener of browser_listeners)
    listener.callback(x);
};

const tabsOnUpdated = function(x) {
  for (const listener of tabs_listeners)
    listener.callback(x);
};

// Browser async API
window.browser.alarms = {
    clearAll: function (args, cb) { return ephy_message ('alarms.clearAll', args, cb); },
};

window.browser.windows = {
    onRemoved: {
      addListener: function (cb) { windows_onremoved_listeners.push({callback: cb}); }
    }
};

window.browser.tabs = {
    create: function (args, cb) { return ephy_message ('tabs.create', args, cb); },
    executeScript: function (...args) { return ephy_message ('tabs.executeScript', args, null); },
    query: function (args, cb) { return ephy_message ('tabs.query', args, cb); },
    get: function (args, cb) { return ephy_message ('tabs.get', args, cb); },
    insertCSS: function (args, cb) { return ephy_message ('tabs.insertCSS', args, cb); },
    removeCSS: function (args, cb) { return ephy_message ('tabs.removeCSS', args, cb); },
    onUpdated: {
      addListener: function (cb) { tabs_listeners.push({callback: cb}); }
    }
};

window.browser.notifications = {
    create: function (args, cb) { return ephy_message ('notifications.create', args, cb); },
};

// browser.runtime is defined in webextensions-common.js
window.browser.runtime.getBrowserInfo = function (args, cb) { return ephy_message ('runtime.getBrowserInfo', args, cb); };
window.browser.runtime.connectNative = function (args, cb) { return ephy_message ('runtime.connectNative', args, cb); };
window.browser.runtime.openOptionsPage = function (args, cb) { return ephy_message ('runtime.openOptionsPage', args, cb); };
window.browser.runtime.setUninstallURL = function (args, cb) { return ephy_message ('runtime.setUninstallURL', args, cb); };
window.browser.runtime.onInstalled = {
    addListener: function (cb) { runtime_listeners.push({callback: cb}); }
};
window.browser.runtime.onMessageExternal = {
    addListener: function (cb) { runtime_onmessageexternal_listeners.push({callback: cb}); }
};

window.browser.pageAction = {
    setIcon: function (args, cb) { return ephy_message ('pageAction.setIcon', args, cb); },
    setTitle: function (args, cb) { return ephy_message ('pageAction.setTitle', args, cb); },
    getTitle: function (args, cb) { return ephy_message ('pageAction.getTitle', args, cb); },
    show: function (args, cb) { return ephy_message ('pageAction.show', args, cb); },
    hide: function (args, cb) { return ephy_message ('pageAction.hide', args, cb); },
    onClicked: {
      addListener: function (cb) { page_listeners.push({callback: cb}); }
    }
};

window.browser.browserAction = {
    onClicked: {
      addListener: function (cb) { browser_listeners.push({callback: cb}); }
    }
};
