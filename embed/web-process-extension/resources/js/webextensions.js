'use strict';

/* exported pageActionOnClicked, browserActionClicked, browserActionClicked, tabsOnUpdated, runtimeSendMessage, runtimeOnConnect */

const promises = [];
let last_promise = 0;

const tabs_listeners = [];
const page_listeners = [];
const browser_listeners = [];
const runtime_listeners = [];
const runtime_onmessage_listeners = [];
const runtime_onmessageexternal_listeners = [];
const runtime_onconnect_listeners = [];
const windows_onremoved_listeners = [];

const ephy_message = function (fn, args, cb) {
    const promise = new Promise (function (resolve, reject) {
        window.webkit.messageHandlers.epiphany.postMessage ({fn: fn, args: args, promise: last_promise});
        last_promise = promises.push({resolve: resolve, reject: reject});
    });
    return promise;
};

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

const runtimeSendMessage = function(x) {
  for (const listener of runtime_onmessage_listeners)
    listener.callback(x);
};

const runtimeOnConnect = function(x) {
  for (const listener of runtime_onconnect_listeners)
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

window.browser.runtime = {
    getManifest: function (args, cb) { return '[]'; },
    getBrowserInfo: function (args, cb) { return ephy_message ('runtime.getBrowserInfo', args, cb); },
    onInstalled: {
      addListener: function (cb) { runtime_listeners.push({callback: cb}); }
    },
    onMessage: {
      addListener: function (cb) { runtime_onmessage_listeners.push({callback: cb}); }
    },
    onMessageExternal: {
      addListener: function (cb) { runtime_onmessageexternal_listeners.push({callback: cb}); }
    },
    onConnect: {
      addListener: function (cb) { runtime_onconnect_listeners.push({callback: cb}); }
    },
    connectNative: function (args, cb) { return ephy_message ('runtime.connectNative', args, cb); },
    sendMessage: function (args, cb) { return ephy_message ('runtime.sendMessage', args, cb); },
    openOptionsPage: function (args, cb) { return ephy_message ('runtime.openOptionsPage', args, cb); },
    setUninstallURL: function (args, cb) { return ephy_message ('runtime.setUninstallURL', args, cb); },
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

// Compatibility with Chrome
window.chrome = window.browser;

