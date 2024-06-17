'use strict';

// Browser async API
window.browser.alarms = {
    clear: function (...args) { return ephy_message ('alarms.clear', args); },
    clearAll: function (...args) { return ephy_message ('alarms.clearAll', args); },
    create: function (...args) { return ephy_message ('alarms.create', args); },
    get: function (...args) { return ephy_message ('alarms.get', args); },
    getAll: function (...args) { return ephy_message ('alarms.getAll', args); },
    onAlarm: new EphyEventListener (),
};

window.browser.commands = {
    getAll: function (...args) { return ephy_message ('commands.getAll', args); },
    reset: function (...args) { return ephy_message ('commands.reset', args); },
    update: function (...args) { return ephy_message ('commands.update', args); },
    onCommand: new EphyEventListener (),
};

window.browser.windows = {
    onRemoved: new EphyEventListener (),
};

window.browser.tabs = {
    create: function (...args) { return ephy_message ('tabs.create', args); },
    executeScript: function (...args) { return ephy_message ('tabs.executeScript', args); },
    query: function (...args) { return ephy_message ('tabs.query', args); },
    get: function (...args) { return ephy_message ('tabs.get', args); },
    getCurrent: function (...args) { return undefined; /* Until we support Option Pages this is correct. */ },
    insertCSS: function (...args) { return ephy_message ('tabs.insertCSS', args); },
    remove: function (...args) { return ephy_message ('tabs.remove', args); },
    removeCSS: function (...args) { return ephy_message ('tabs.removeCSS', args); },
    sendMessage: function (...args) { return ephy_message ('tabs.sendMessage', args); },
    update: function (...args) { return ephy_message ('tabs.update', args); },
    getZoom: function (...args) { return ephy_message ('tabs.getZoom', args); },
    setZoom: function (...args) { return ephy_message ('tabs.setZoom', args); },
    reload: function (...args) { return ephy_message ('tabs.reload', args); },
    onActivated: new EphyEventListener (),
    onAttached: new EphyEventListener (),
    onCreated: new EphyEventListener (),
    onDetached: new EphyEventListener (),
    onHighlighted: new EphyEventListener (),
    onMoved: new EphyEventListener (),
    onRemoved: new EphyEventListener (),
    onUpdated: new EphyEventListener (),
    onZoomChange: new EphyEventListener (),
    onReplaced: new EphyEventListener (),
    TAB_ID_NONE: -1,
};

window.browser.notifications = {
    clear: function (...args) { return ephy_message ('notifications.clear', args); },
    create: function (...args) { return ephy_message ('notifications.create', args); },
    update: function (...args) { return ephy_message ('notifications.update', args); },
    onClicked: new EphyEventListener (),
    onButtonClicked: new EphyEventListener (),
    // The remaining APIs here are stubs as long as we use GNotification since we don't have this information.
    getAll: function (...args) { return ephy_message ('notifications.getAll', args); },
    onClosed: new EphyEventListener (),
    onShown: new EphyEventListener (),
};

// browser.extension is defined in ephy-webextension-common.c
window.browser.extension.getViews = function (fetchProperties) {
    const window_objects = window.browser.extension._ephy_get_view_objects();
    if (!window_objects || !fetchProperties)
        return window_objects;

    // TODO: Implement actual filtering.
    if (fetchProperties.type === 'background')
        return [window_objects[0]];
    else if (fetchProperties.type === 'popup')
        return window_objects.slice(1);
    else if (fetchProperties.type !== undefined)
        return [];

    return window_objects;
};
window.browser.extension.getBackgroundPage = function () {
    const views = window.browser.extension.getViews({type: 'background'});
    if (!views)
        return null;
    return views[0];
};

// browser.runtime is defined in webextensions-common.js
window.browser.runtime.getBrowserInfo = function (...args) { return ephy_message ('runtime.getBrowserInfo', args); };
window.browser.runtime.getPlatformInfo = function (...args) { return ephy_message ('runtime.getPlatformInfo', args); };
window.browser.runtime.connectNative = function (...args) { return ephy_message ('runtime.connectNative', args); };
window.browser.runtime.openOptionsPage = function (...args) { return ephy_message ('runtime.openOptionsPage', args); };
window.browser.runtime.setUninstallURL = function (...args) { return ephy_message ('runtime.setUninstallURL', args); };
window.browser.runtime.onInstalled = new EphyEventListener ();
window.browser.runtime.onMessageExternal = new EphyEventListener ();
window.browser.runtime.sendNativeMessage = function (...args) { return ephy_message ('runtime.sendNativeMessage', args); };
Object.defineProperty(window.browser.runtime, 'lastError', { get: function() { return window.browser.extension.lastError; } });
window.browser.runtime.getBackgroundPage = function () { return Promise.resolve (window.browser.extension.getBackgroundPage ()); };

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
    setBadgeText: function (...args) { return ephy_message ('browserAction.setBadgeText', args); },
    setBadgeBackgroundColor: function (...args) { return ephy_message ('browserAction.setBadgeBackgroundColor', args); },
};

window.browser.permissions = {
    contains: function (...args) { return ephy_message ('permissions.contains', args); },
    getAll: function (...args) { return ephy_message ('permissions.getAll', args); },
    remove: function (...args) { return ephy_message ('permissions.remove', args); },
    request: function (...args) { return ephy_message ('permissions.request', args); },
    onAdded: new EphyEventListener (),
    onRemoved: new EphyEventListener (),
};

window.browser.windows = {
    get: function (...args) { return ephy_message ('windows.get', args); },
    getCurrent: function (...args) { return ephy_message ('windows.getCurrent', args); },
    getLastFocused: function (...args) { return ephy_message ('windows.getLastFocused', args); },
    getAll: function (...args) { return ephy_message ('windows.getAll', args); },
    create: function (...args) { return ephy_message ('windows.create', args); },
    update: function (...args) { return ephy_message ('windows.update', args); },
    remove: function (...args) { return ephy_message ('windows.remove', args); },
    onCreated: new EphyEventListener (),
    onRemoved: new EphyEventListener (),
    onFocusChanged: new EphyEventListener (),
    WINDOW_ID_CURRENT: -2, /* Matches Firefox, used in tabs.c. */
};

window.browser.cookies = {
    get: function (...args) { return ephy_message ('cookies.get', args); },
    getAll: function (...args) { return ephy_message ('cookies.getAll', args); },
    getAllCookieStores: function (...args) { return ephy_message ('cookies.getAllCookieStores', args); },
    remove: function (...args) { return ephy_message ('cookies.remove', args); },
    set: function (...args) { return ephy_message ('cookies.set', args); },
    // This is a stub as WebKitCookieManager::changed doesn't tell us enough information.
    onChanged: new EphyEventListener (),
};

window.browser.downloads = {
    download: function (...args) { return ephy_message ('downloads.download', args); },
    // FIXME: In the query object for search and erase, convert JavaScript Date objects to timestamps.
    search: function (...args) { return ephy_message ('downloads.search', args); },
    erase: function (...args) { return ephy_message ('downloads.erase', args); },
    pause: function (...args) { return ephy_message ('downloads.pause', args); },
    resume: function (...args) { return ephy_message ('downloads.resume', args); },
    cancel: function (...args) { return ephy_message ('downloads.cancel', args); },
    getFileIcon: function (...args) { return ephy_message ('downloads.getFileIcon', args); },
    open: function (...args) { return ephy_message ('downloads.open', args); },
    show: function (...args) { return ephy_message ('downloads.show', args); },
    showDefaultFolder: function (...args) { return ephy_message ('downloads.showDefaultFolder', args); },
    removeFile: function (...args) { return ephy_message ('downloads.removeFile', args); },
    onCreated: new EphyEventListener (),
    onErased: new EphyEventListener (),
    onChanged: new EphyEventListener (),
};

window.browser.menus = {
    create: function (...args) { return ephy_message ('menus.create', args); },
    remove: function (...args) { return ephy_message ('menus.remove', args); },
    removeAll: function (...args) { return ephy_message ('menus.removeAll', args); },
    update: function (...args) { return ephy_message ('menus.update', args); },
    onClicked: new EphyEventListener (),
};

// Chrome compat.
window.browser.contextMenus = window.browser.menus;
