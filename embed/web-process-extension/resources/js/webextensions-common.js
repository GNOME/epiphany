'use strict';

/* exported ephy_message */
/* global ephy_send_message */

window.browser = {};

class EphyEventListener {
    constructor () {
        this._listeners = [];
    }

    addListener (cb) {
        this._listeners.push({callback: cb});
    }

    removeListener (cb) {
        this._listeners = this._listeners.filter(l => l.callback !== cb);
    }

    hasListener (cb) {
        return !!this._listeners.find(l => l.callback === cb);
    }

    _emit (data) {
        for (const listener of this._listeners)
            listener.callback (data);
    }
}

const ephy_message = function (fn, ...args) {
    return new Promise (function (resolve, reject) {
        ephy_send_message (fn, args, resolve, reject);
    });
};

window.browser.runtime = {
    getURL: function (args, cb) { return window.browser.extension.getURL(args, cb); },
    getManifest: function (args, cb) { return '[]'; },
    onMessage: new EphyEventListener (),
    onConnect: new EphyEventListener (),
    sendMessage: function (args, cb) {
        return ephy_message ('runtime.sendMessage', args, cb);
    },
};


window.browser.storage = {
    local: {
        get: function (keys) {
            return ephy_message ('storage.local.get', keys);
        },
        set: function (keys) {
            return ephy_message ('storage.local.set', keys);
        },
        remove: function (keys) {
            return ephy_message ('storage.local.remove', keys);
        },
        clear: function () {
            return ephy_message ('storage.local.clear');
        }
    }
};

// Compatibility with Chrome
window.chrome = window.browser;
